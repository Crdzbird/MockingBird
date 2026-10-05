// FIR downsampler by an integer factor. Consumes M inputs per output; the
// input block length must be a multiple of M.
#pragma once

#include "preprocess/lowpass_design.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace auris {

class Decimator {
public:
    void configure(int factor, size_t taps_per_phase = 16) {
        m_ = factor < 1 ? 1 : factor;
        if (m_ == 1) {
            proto_.clear();
            hist_.clear();
            return;
        }
        const size_t taps = (taps_per_phase * static_cast<size_t>(m_)) | 1u;
        proto_ = design_lowpass(taps, 0.5 / static_cast<double>(m_), 1.0);
        hist_.assign(taps, 0.0f);
        phase_ = 0;
        pos_   = 0;
    }

    void process(const float* in, size_t n, std::vector<float>& out) {
        if (m_ == 1) {
            out.insert(out.end(), in, in + n);
            return;
        }
        const size_t taps = hist_.size();
        for (size_t i = 0; i < n; ++i) {
            const size_t cur = pos_;
            hist_[pos_] = in[i];
            pos_ = (pos_ + 1 == taps) ? 0 : pos_ + 1;
            if (++phase_ != m_) continue;
            phase_ = 0;
            double acc = 0.0;
            for (size_t k = 0; k < proto_.size(); ++k) {
                const size_t idx = (cur + taps - k) % taps;
                acc += static_cast<double>(proto_[k]) * static_cast<double>(hist_[idx]);
            }
            out.push_back(static_cast<float>(acc));
        }
    }

    void reset() {
        std::fill(hist_.begin(), hist_.end(), 0.0f);
        phase_ = 0;
        pos_   = 0;
    }

private:
    int                m_     = 1;
    int                phase_ = 0;
    size_t             pos_   = 0;
    std::vector<float> proto_;
    std::vector<float> hist_;
};

} // namespace auris
