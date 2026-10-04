// Adaptive degradation policy: a pure state machine fed one busy figure per
// worker-health window. Escalation is immediate, de-escalation needs three
// calm windows below a lower threshold, so load near the limit settles.
#pragma once

#include <cstdint>

namespace auris {

class DegradationPolicy {
public:
    static constexpr uint32_t kMaxLevel    = 3;
    static constexpr uint32_t kCalmWindows = 3;

    void configure(uint32_t busy_high_ppm) {
        high_ = busy_high_ppm == 0 ? 700000u : busy_high_ppm;
        if (high_ > 1000000u) high_ = 1000000u;
        low_ = high_ * 2u / 5u;
    }

    bool update(uint32_t busy_ppm, bool dropped) {
        if (busy_ppm > high_ || dropped) {
            calm_ = 0;
            if (level_ < kMaxLevel) { ++level_; return true; }
            return false;
        }
        if (busy_ppm < low_) {
            if (level_ == 0) return false;
            if (++calm_ >= kCalmWindows) { --level_; calm_ = 0; return true; }
            return false;
        }
        calm_ = 0;
        return false;
    }

    uint32_t level() const { return level_; }
    uint32_t cadence_multiplier() const { return level_ >= kMaxLevel ? 4u : (1u << level_); }
    bool     inhibit() const { return level_ >= kMaxLevel; }

private:
    uint32_t high_  = 700000u;
    uint32_t low_   = 280000u;
    uint32_t level_ = 0;
    uint32_t calm_  = 0;
};

} // namespace auris
