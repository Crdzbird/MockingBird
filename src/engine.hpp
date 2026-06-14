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

    auris_engine(const auris_config_t& c,
                 size_t                ring_frames,
                 size_t                pool_capacity,
                 size_t                max_payload_bytes)
        : cfg(c),
          pcm_ring(ring_frames),
          packets(pool_capacity, max_payload_bytes) {}
};
