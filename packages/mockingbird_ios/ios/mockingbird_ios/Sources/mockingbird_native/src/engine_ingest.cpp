// Lock-free and allocation-free: a ring push, a few atomics, one wake.
#include "engine_ingest.hpp"

#include "engine.hpp"

namespace mockingbird {

void ingest_pcm(mockingbird_engine* e, const int16_t* samples, uint32_t frame_count,
                uint64_t capture_ts_us) noexcept {
    e->last_capture_ts_us.store(capture_ts_us, std::memory_order_release);
    uint64_t expected_first = ~uint64_t{0};
    e->first_capture_ts_us.compare_exchange_strong(
        expected_first, capture_ts_us, std::memory_order_relaxed);

    const size_t written = e->pcm_ring.push(samples, static_cast<size_t>(frame_count));
    e->frames_captured.fetch_add(written, std::memory_order_relaxed);
    if (written < frame_count) {
        e->frames_dropped_capture.fetch_add(frame_count - written,
                                            std::memory_order_relaxed);
    }

    const size_t batch = e->wake_batch_samples.load(std::memory_order_relaxed);
    if (batch != 0 && written == frame_count &&
        e->pcm_ring.size_relaxed() < batch) {
        return;
    }
    e->wake_count.fetch_add(1, std::memory_order_release);
    e->wake_count.notify_one();
}

void backend_pcm_callback(void* user, const int16_t* samples,
                          uint32_t frame_count, uint64_t capture_ts_us) noexcept {
    ingest_pcm(static_cast<mockingbird_engine*>(user), samples, frame_count, capture_ts_us);
}

} // namespace mockingbird
