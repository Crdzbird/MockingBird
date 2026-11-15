// Lock-free and allocation-free: a ring push, a few atomics, one wake. A
// capture backend's chunks also feed the gap estimate: the capture clock
// against the audio delivered since the last anchor, re-anchored on a gap
// and whenever chunks run early, so jitter never accumulates.
#include "engine_ingest.hpp"

#include "engine.hpp"
#include "engine_events.hpp"

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

void track_capture_gap(mockingbird_engine* e, uint64_t capture_ts_us,
                       uint32_t frame_count) noexcept {
    constexpr auto relaxed = std::memory_order_relaxed;
    const uint64_t rate   = e->cfg.sample_rate_hz;
    const uint64_t anchor = e->capture_anchor_us.load(relaxed);
    const uint64_t frames = e->capture_anchor_frames.load(relaxed);
    bool reanchor = anchor == ~uint64_t{0} || rate == 0;
    if (!reanchor) {
        const uint64_t expected = anchor + frames * 1'000'000ull / rate;
        if (capture_ts_us > expected + kCaptureGapUs) {
            const uint64_t lost_ms = (capture_ts_us - expected) / 1000ull;
            e->capture_gaps.fetch_add(1, relaxed);
            e->capture_gap_ms.fetch_add(static_cast<uint32_t>(lost_ms), relaxed);
            reanchor = true;
        } else if (capture_ts_us + kCaptureGapUs < expected) {
            reanchor = true;
        }
    }
    if (reanchor) {
        e->capture_anchor_us.store(capture_ts_us, relaxed);
        e->capture_anchor_frames.store(frame_count, relaxed);
        return;
    }
    e->capture_anchor_frames.store(frames + frame_count, relaxed);
}

void backend_pcm_callback(void* user, const int16_t* samples,
                          uint32_t frame_count, uint64_t capture_ts_us) noexcept {
    auto* e = static_cast<mockingbird_engine*>(user);
    track_capture_gap(e, capture_ts_us, frame_count);
    ingest_pcm(e, samples, frame_count, capture_ts_us);
}

void backend_notice_callback(void* user, mockingbird_event_type_t type,
                             uint32_t reason) noexcept {
    auto* e = static_cast<mockingbird_engine*>(user);
    if (type == MOCKINGBIRD_EVT_CAPTURE_RESUMED || type == MOCKINGBIRD_EVT_ROUTE_CHANGED) {
        e->capture_restarts.fetch_add(1, std::memory_order_relaxed);
    }
    try {
        emit_event(e, make_event(type, 0, e->last_capture_ts_us.load(std::memory_order_relaxed),
                                 reason));
    } catch (...) {
    }
}

}
