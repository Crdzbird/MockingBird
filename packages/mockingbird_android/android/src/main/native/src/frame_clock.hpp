// Deterministic per-frame timestamps: the first pushed frame's capture time
// plus the frame index times the frame length, so identical input always
// yields identical timestamps regardless of thread timing.
#pragma once

#include <atomic>
#include <cstdint>

namespace mockingbird {
class FrameClock {
public:
    static constexpr uint64_t kUnset = ~uint64_t{0};

    FrameClock(const std::atomic<uint64_t>& first_capture_ts_us, uint32_t frame_ms)
        : first_ts_(first_capture_ts_us),
          frame_us_(static_cast<uint64_t>(frame_ms) * 1000ull) {}

    uint64_t now() const {
        uint64_t base = first_ts_.load(std::memory_order_relaxed);
        if (base == kUnset) base = 0;
        return base + index_ * frame_us_;
    }

    uint64_t next() {
        const uint64_t ts = now();
        ++index_;
        return ts;
    }

private:
    const std::atomic<uint64_t>& first_ts_;
    uint64_t                     frame_us_;
    uint64_t                     index_ = 0;
};

}
