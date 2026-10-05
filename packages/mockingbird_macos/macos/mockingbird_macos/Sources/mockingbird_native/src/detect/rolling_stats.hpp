// Sliding-window mean and variance with O(1) push and z-score. A small
// floor on sigma keeps a burst after pure silence measurable.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace mockingbird {

class RollingStats {
public:
    void init(size_t window) {
        window_ = std::max<size_t>(window, 4);
        ring_.assign(window_, 0.0f);
        idx_    = 0;
        filled_ = 0;
        sum_    = 0.0;
        sumsq_  = 0.0;
    }

    void push(float v) {
        const double dv = static_cast<double>(v);
        if (filled_ < window_) {
            ring_[idx_] = v;
            sum_   += dv;
            sumsq_ += dv * dv;
            ++filled_;
        } else {
            const double dold = static_cast<double>(ring_[idx_]);
            ring_[idx_] = v;
            sum_   += dv - dold;
            sumsq_ += dv * dv - dold * dold;
        }
        idx_ = (idx_ + 1) % window_;
    }

    float z_score(float v) const {
        if (filled_ < window_ / 2) return 0.0f;
        const double mean  = sum_ / static_cast<double>(filled_);
        const double var   = std::max(0.0, sumsq_ / static_cast<double>(filled_) - mean * mean);
        const double sigma = std::sqrt(var) + 1e-6;
        return static_cast<float>((static_cast<double>(v) - mean) / sigma);
    }

private:
    std::vector<float> ring_;
    size_t             window_ = 0;
    size_t             idx_    = 0;
    size_t             filled_ = 0;
    double             sum_    = 0.0;
    double             sumsq_  = 0.0;
};

} // namespace mockingbird
