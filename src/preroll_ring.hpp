// src/preroll_ring.hpp
//
// Fixed-capacity circular buffer of recent captured frames. Lives entirely
// on the worker thread (T2); no synchronization needed.
//
// Detectors fire AFTER speech / anomaly onset, so the first ~100..300 ms of
// every segment would otherwise be lost. The worker pushes every captured
// frame here while no segment is open; on a rising edge it flushes the ring
// into the new segment as PREROLL-flagged packets, then clears.
//
// Memory: K frames * frame_samples * 2 bytes. At defaults (300 ms / 20 ms
// frames at 16 kHz mono) that's 15 * 320 * 2 = 9.6 KB. Negligible.

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace auris {

class PrerollRing {
public:
    struct Frame {
        std::vector<int16_t> samples;
        uint64_t             capture_ts_us = 0;
    };

    // capacity_frames may be 0 (preroll disabled). frame_samples is the size
    // of every push() and the expected buffer length on read.
    void init(size_t capacity_frames, size_t frame_samples) {
        ring_.clear();
        ring_.resize(capacity_frames);
        for (auto& f : ring_) f.samples.assign(frame_samples, 0);
        frame_samples_ = frame_samples;
        head_   = 0;
        filled_ = 0;
    }

    bool empty()   const noexcept { return filled_ == 0; }
    size_t size()  const noexcept { return filled_; }
    size_t capacity() const noexcept { return ring_.size(); }

    void clear() noexcept {
        head_   = 0;
        filled_ = 0;
    }

    // Overwrite the oldest slot with this frame. Safe to call when capacity
    // is zero (becomes a no-op).
    void push(const int16_t* samples, uint64_t capture_ts_us) noexcept {
        if (ring_.empty() || frame_samples_ == 0) return;
        auto& slot = ring_[head_];
        std::memcpy(slot.samples.data(), samples, frame_samples_ * sizeof(int16_t));
        slot.capture_ts_us = capture_ts_us;
        head_ = (head_ + 1) % ring_.size();
        if (filled_ < ring_.size()) ++filled_;
    }

    // Iterate from oldest to newest. Invokes f(const Frame&) for each.
    // Caller is expected to consume each frame (typically emit as a packet).
    template <typename F>
    void for_each_oldest_first(F&& f) const {
        if (filled_ == 0) return;
        const size_t start =
            (head_ + ring_.size() - filled_) % ring_.size();
        for (size_t i = 0; i < filled_; ++i) {
            const size_t idx = (start + i) % ring_.size();
            f(ring_[idx]);
        }
    }

    // Timestamp of the oldest stored frame, or 0 if empty.
    uint64_t oldest_ts_us() const noexcept {
        if (filled_ == 0) return 0;
        const size_t start =
            (head_ + ring_.size() - filled_) % ring_.size();
        return ring_[start].capture_ts_us;
    }

private:
    std::vector<Frame> ring_;
    size_t             frame_samples_ = 0;
    size_t             head_          = 0;
    size_t             filled_        = 0;
};

} // namespace auris
