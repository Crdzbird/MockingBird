// src/engine.hpp
//
// Internal definition of struct auris_engine (opaque to the ABI).

#pragma once

#include "auris/auris.h"
#include "capture/capture_backend.hpp"
#include "encode/encoder.hpp"
#include "packet_pool.hpp"
#include "spsc_ring.hpp"

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

struct auris_engine {
    auris_config_t cfg;

    // // CONTRACT  Owned copies of the caller's model-path strings. The
    // auris_config_t the caller passes only carries BORROWED const char*
    // pointers, valid only for the duration of auris_create. The worker
    // thread reads these long after auris_create returns, so the engine
    // MUST own its own copies and repoint cfg at them. Without this the
    // worker dereferences freed memory (use-after-free across the ABI).
    std::string neural_vad_model_path_storage;
    std::string sound_class_model_path_storage;

    // T1 -> T2 PCM ring (int16, mono v1).
    auris::SpscRing<int16_t> pcm_ring;

    // T2 -> T3 encoded packet pool + ready queue.
    auris::PacketPool packets;

    // Live capture backend (null in HOST_PCM mode). Constructed in
    // auris_create; init+start in auris_start; stop in auris_stop;
    // destroyed with the engine.
    std::unique_ptr<auris::CaptureBackend> capture_backend;

    // Frame encoder. Constructed + init'd in auris_create so the pool can be
    // sized from max_packet_bytes(). Lives on the worker thread.
    std::unique_ptr<auris::Encoder> encoder;

    // Worker thread (T2). Plain std::thread because Android's NDK libc++
    // does not (yet) ship std::jthread. The engine carries its own
    // stop_requested atomic and wake_count signal, so we never needed
    // std::stop_token for correctness; jthread was only a convenience.
    std::thread        worker_thread;
    std::atomic<bool>  running{false};
    std::atomic<bool>  stop_requested{false};
    // Set if the worker thread died to an uncaught exception (e.g. bad_alloc).
    // The worker catches it, emits AURIS_EVT_BACKEND_ERROR, and exits rather than
    // letting the exception escape the thread and call std::terminate. A fatal
    // engine rejects further auris_push_pcm.
    std::atomic<bool>  worker_fatal{false};
    // Serializes the start/stop/destroy teardown transition so two concurrent
    // stops (or a stop racing a start rollback) cannot both pass the joinable()
    // check and double-join the worker (UB). Does NOT make auris_destroy safe to
    // call concurrently with other entry points — destroy frees the engine, so
    // the host must still ensure no other call is in flight (see auris.h).
    std::mutex         lifecycle_mu;

    // Live-tunable config. Starts as a copy of `cfg`; auris_update_config
    // writes ONLY the tunable subset here (thresholds, detector enables) under
    // tune_mu and bumps tune_generation. The worker polls the generation once
    // per frame (one relaxed atomic load) and takes the mutex only when it
    // actually changed. `cfg` itself stays immutable for the engine's lifetime,
    // so every structural field the worker reads unlocked remains race-free.
    std::mutex            tune_mu;
    auris_config_t        live_cfg{};
    std::atomic<uint64_t> tune_generation{0};

    // Lock-free wake signal. Producer (T1 or host's push thread) increments
    // and notify_one(); worker waits on a stale value.
    std::atomic<uint64_t> wake_count{0};

    // Event callback registration. Mutex on set; worker takes a snapshot
    // under the same mutex. Uncontended in steady state.
    std::mutex     cb_mu;
    auris_event_cb event_cb   = nullptr;
    void*          event_user = nullptr;

    // Pull-mode event queue.
    std::mutex                evt_mu;
    std::deque<auris_event_t> evt_queue;

    // Stats (relaxed; observers tolerate skew).
    std::atomic<uint64_t> frames_captured{0};
    std::atomic<uint64_t> frames_dropped_capture{0};
    std::atomic<uint64_t> packets_emitted{0};
    std::atomic<uint64_t> packets_dropped{0};
    std::atomic<uint64_t> segments_opened{0};
    std::atomic<uint64_t> segments_closed{0};

    // Worker-only state (no concurrent access).
    uint64_t next_segment_id = 1;

    // Most recent capture timestamp seen, in microseconds. Used to stamp
    // segment events when no PCM is in hand.
    std::atomic<uint64_t> last_capture_ts_us{0};

    // Timestamp of the very first frame pushed since the last start (sentinel
    // UINT64_MAX until set). Per-frame packet/event timestamps are derived
    // deterministically as first_capture_ts_us + frame_index * frame_us, so they
    // reflect each frame's own capture time and do NOT depend on the race between
    // the producer and the worker — identical input yields identical timestamps.
    std::atomic<uint64_t> first_capture_ts_us{~uint64_t{0}};

    // Input level of the most recent frame, in dBFS, measured before noise
    // suppression. Written by the worker, read by auris_get_stats.
    std::atomic<float>    input_rms_dbfs{0.0f};
    std::atomic<float>    input_peak_dbfs{0.0f};

    // Continuous (ungated) PCM tap. Non-null only when cfg.enable_pcm_tap is
    // set. Producer is the worker; consumer is the host via auris_read_pcm —
    // the same SPSC contract as the capture ring.
    std::unique_ptr<auris::SpscRing<int16_t>> pcm_tap;
    std::atomic<uint64_t> tap_samples_dropped{0};

    // Worker health + work-shedding counters (auris_stats_t). Written only by
    // the worker; relaxed loads from auris_get_stats.
    std::atomic<uint32_t> worker_busy_ppm{0};
    std::atomic<uint32_t> worker_max_frame_us{0};
    std::atomic<uint64_t> worker_wakeups{0};
    std::atomic<uint64_t> sound_class_inferences{0};
    std::atomic<uint64_t> sound_class_inferences_skipped{0};
    std::atomic<uint64_t> preproc_frames_skipped{0};
    std::atomic<uint32_t> degradation_level{0};

    // Wake batching threshold in SAMPLES (worker_batch_frames * frame). Read
    // relaxed on the capture thread per push; 0 => wake on every push.
    std::atomic<size_t>   wake_batch_samples{0};

    auris_engine(const auris_config_t& c,
                 size_t                ring_frames,
                 size_t                pool_capacity,
                 size_t                max_payload_bytes)
        : cfg(c),
          pcm_ring(ring_frames),
          packets(pool_capacity, max_payload_bytes),
          live_cfg(c) {}   // declaration order: cfg, pcm_ring, packets, …, live_cfg
};
