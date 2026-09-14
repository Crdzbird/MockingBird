// src/degrade.hpp
//
// Adaptive degradation policy: decides, once per worker-health window, whether
// to shed or restore optional work. Pure state machine — no clocks, no engine
// access — so it is unit-tested with synthetic inputs and the worker only has
// to feed it one number per window.
//
// Escalation is immediate: a window already over budget means audio is one bad
// window away from being dropped, so waiting for confirmation costs frames.
// De-escalation is deliberate: three consecutive calm windows, and the calm
// threshold sits well below the busy one, so a load that hovers near the limit
// settles at a level instead of flapping around it.

#pragma once

#include <cstdint>

namespace auris {

class DegradationPolicy {
public:
    static constexpr uint32_t kMaxLevel     = 3;
    static constexpr uint32_t kCalmWindows  = 3;

    // busy_high_ppm: escalate when a window's busy share exceeds this.
    // The calm threshold is derived (40% of it) so there is a single knob.
    void configure(uint32_t busy_high_ppm) {
        high_ = busy_high_ppm == 0 ? 700000u : busy_high_ppm;
        if (high_ > 1000000u) high_ = 1000000u;
        low_  = high_ * 2u / 5u;
    }

    // One call per closed window. `dropped` = the capture ring lost frames
    // during the window, which is overload by definition regardless of busy
    // share. Returns true when the level changed.
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
        calm_ = 0;          // between thresholds: hold
        return false;
    }

    uint32_t level() const { return level_; }

    // Inference cadence multiplier for the sound classifier at this level;
    // level 3 is "suspended" and handled by the caller via inhibit().
    uint32_t cadence_multiplier() const { return level_ >= kMaxLevel ? 4u : (1u << level_); }
    bool     inhibit() const            { return level_ >= kMaxLevel; }

private:
    uint32_t high_  = 700000u;
    uint32_t low_   = 280000u;
    uint32_t level_ = 0;
    uint32_t calm_  = 0;
};

} // namespace auris
