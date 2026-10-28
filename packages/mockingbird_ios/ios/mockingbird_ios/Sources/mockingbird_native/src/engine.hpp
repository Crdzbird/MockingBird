// The engine behind the opaque mockingbird_engine_t handle: queues, threads,
// counters, and the live-tunable copy of the configuration.
#pragma once

#include "mockingbird/mockingbird.h"
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

namespace mockingbird {
class Worker;
}

struct mockingbird_engine {
    mockingbird_config_t cfg;
    std::string    neural_vad_model_path_storage;
    std::string    sound_class_model_path_storage;

    mockingbird::SpscRing<int16_t> pcm_ring;
    mockingbird::PacketPool        packets;

    std::unique_ptr<mockingbird::CaptureBackend> capture_backend;
    bool capture_backend_used = false;
    std::string input_device_id;
    std::atomic<uint32_t> voice_processing_active{0};
    std::atomic<uint32_t> capture_restarts{0};
    std::unique_ptr<mockingbird::Encoder>        encoder;

    std::thread                    worker_thread;
    std::unique_ptr<mockingbird::Worker> inline_worker;
    std::atomic<bool> running{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> worker_fatal{false};
    std::mutex        lifecycle_mu;

    std::mutex            tune_mu;
    mockingbird_config_t        live_cfg{};
    std::atomic<uint64_t> tune_generation{0};

    std::atomic<uint64_t> wake_count{0};
    std::atomic<size_t>   wake_batch_samples{0};

    std::mutex     cb_mu;
    mockingbird_event_cb event_cb   = nullptr;
    void*          event_user = nullptr;

    std::mutex          wake_mu;
    mockingbird_wake_cb wake_cb   = nullptr;
    void*               wake_user = nullptr;
    std::atomic<bool>   wake_pending{false};

    std::mutex                evt_mu;
    std::deque<mockingbird_event_t> evt_queue;

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

    std::unique_ptr<mockingbird::SpscRing<int16_t>> pcm_tap;
    std::atomic<uint64_t>                     tap_samples_dropped{0};

    std::atomic<uint32_t> worker_busy_ppm{0};
    std::atomic<uint32_t> worker_max_frame_us{0};
    std::atomic<uint64_t> worker_wakeups{0};
    std::atomic<uint64_t> sound_class_inferences{0};
    std::atomic<uint64_t> sound_class_inferences_skipped{0};
    std::atomic<uint64_t> preproc_frames_skipped{0};
    std::atomic<uint32_t> degradation_level{0};

    mockingbird_engine(const mockingbird_config_t& c,
                 size_t                ring_frames,
                 size_t                pool_capacity,
                 size_t                max_payload_bytes);
    ~mockingbird_engine();
};
