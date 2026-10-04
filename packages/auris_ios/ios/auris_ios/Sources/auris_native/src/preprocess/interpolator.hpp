// Polyphase upsampler by an integer factor. Emits L outputs per input and
// keeps FIR history across calls so block processing is continuous.
#pragma once

#include "preprocess/lowpass_design.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace auris {

class Interpolator {
public:
    void configure(int factor, size_t taps_per_phase = 16) {
        l_ = factor < 1 ? 1 : factor;
        if (l_ == 1) {
            proto_.clear();
            hist_.clear();
            return;
        }
        taps_per_phase_ = taps_per_phase;
        const size_t taps = taps_per_phase_ * static_cast<size_t>(l_);
        proto_ = design_lowpass(taps, 0.5 / static_cast<double>(l_), static_cast<double>(l_));
        hist_.assign(taps_per_phase_, 0.0f);
        pos_ = 0;
    }

    void process(const float* in, size_t n, std::vector<float>& out) {
        if (l_ == 1) {
            out.insert(out.end(), in, in + n);
            return;
        }
        const size_t taps = hist_.size();
        for (size_t i = 0; i < n; ++i) {
            hist_[pos_] = in[i];
            for (size_t p = 0; p < static_cast<size_t>(l_); ++p) {
                double acc = 0.0;
                for (size_t k = 0; k < taps_per_phase_; ++k) {
                    const size_t idx = (pos_ + taps - k) % taps;
                    acc += static_cast<double>(proto_[p + static_cast<size_t>(l_) * k]) *
                           static_cast<double>(hist_[idx]);
                }
                out.push_back(static_cast<float>(acc));
            }
            pos_ = (pos_ + 1 == taps) ? 0 : pos_ + 1;
        }
    }

    void reset() {
        std::fill(hist_.begin(), hist_.end(), 0.0f);
        pos_ = 0;
    }

private:
    int                l_              = 1;
    size_t             taps_per_phase_ = 0;
    size_t             pos_            = 0;
    std::vector<float> proto_;
    std::vector<float> hist_;
};

} // namespace auris
