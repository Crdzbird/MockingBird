// The engine behind the opaque auris_engine_t handle: queues, threads,
// counters, and the live-tunable copy of the configuration.
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
    std::string    neural_vad_model_path_storage;
    std::string    sound_class_model_path_storage;

    auris::SpscRing<int16_t> pcm_ring;
    auris::PacketPool        packets;

    std::unique_ptr<auris::CaptureBackend> capture_backend;
    std::unique_ptr<auris::Encoder>        encoder;

    std::thread       worker_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> worker_fatal{false};
    std::mutex        lifecycle_mu;

    std::mutex            tune_mu;
    auris_config_t        live_cfg{};
    std::atomic<uint64_t> tune_generation{0};

    std::atomic<uint64_t> wake_count{0};
    std::atomic<size_t>   wake_batch_samples{0};

    std::mutex     cb_mu;
    auris_event_cb event_cb   = nullptr;
    void*          event_user = nullptr;

    std::mutex                evt_mu;
    std::deque<auris_event_t> evt_queue;

    std::atomic<uint64_t> frames_captured{0};
    std::atomic<uint64_t> frames_dropped_capture{0};
    std::atomic<uint64_t> packets_emitted{0};
    std::atomic<uint64_t> packets_dropped{0};
    std::atomic<uint64_t> segments_opened{0};
    std::atomic<uint64_t> segments_closed{0};
    uint64_t              next_segment_id = 1;

    std::atomic<uint64_t> last_capture_ts_us{0};
    std::atomic<uint64_t> first_capture_ts_us{~uint64_t{0}};
    std::atomic<float>    input_rms_dbfs{0.0f};
    std::atomic<float>    input_peak_dbfs{0.0f};

    std::unique_ptr<auris::SpscRing<int16_t>> pcm_tap;
    std::atomic<uint64_t>                     tap_samples_dropped{0};

    std::atomic<uint32_t> worker_busy_ppm{0};
    std::atomic<uint32_t> worker_max_frame_us{0};
    std::atomic<uint64_t> worker_wakeups{0};
    std::atomic<uint64_t> sound_class_inferences{0};
    std::atomic<uint64_t> sound_class_inferences_skipped{0};
    std::atomic<uint64_t> preproc_frames_skipped{0};
    std::atomic<uint32_t> degradation_level{0};

    auris_engine(const auris_config_t& c,
                 size_t                ring_frames,
                 size_t                pool_capacity,
                 size_t                max_payload_bytes)
        : cfg(c),
          pcm_ring(ring_frames),
          packets(pool_capacity, max_payload_bytes),
          live_cfg(c) {}
};
