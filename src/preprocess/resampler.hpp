// src/preprocess/resampler.hpp
//
// Integer-ratio polyphase FIR resampling, just enough to bridge the engine's
// working rate (8 k / 16 k) to RNNoise's required 48 kHz and back.
//
//   Interpolator(L)  upsamples by an integer factor L (anti-imaging LPF).
//   Decimator(M)     downsamples by an integer factor M (anti-aliasing LPF).
//
// Both are stateful: they carry FIR history across calls so block-by-block
// processing of a continuous stream is click-free. Prototype filters are
// windowed-sinc (Hann), designed once at configure().
//
// // NOTE  This is a detection front-end, not a hi-fi SRC: ~16 taps/phase with
// a Hann window gives ~ -40 dB stopband, which is ample for cleaning the
// 0..(working Nyquist) band that RNNoise actually denoises. The upper 48 kHz
// bands carry essentially no energy (the source is band-limited) so RNNoise's
// gains there are inert.

#ifndef AURIS_PREPROCESS_RESAMPLER_HPP
#define AURIS_PREPROCESS_RESAMPLER_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace auris {

namespace detail {

// Windowed-sinc low-pass prototype. fc is the cutoff in cycles/sample
// (0 < fc < 0.5) relative to the rate the filter RUNS at. The returned taps
// sum to `dc_gain` so the passband settles at that gain.
inline std::vector<float> design_lowpass(size_t taps, double fc, double dc_gain) {
    std::vector<float> h(taps, 0.0f);
    const double center = static_cast<double>(taps - 1) / 2.0;
    const double pi = 3.14159265358979323846;
    double sum = 0.0;
    for (size_t n = 0; n < taps; ++n) {
        const double m = static_cast<double>(n) - center;
        const double sinc = (std::fabs(m) < 1e-9)
            ? 2.0 * fc
            : std::sin(2.0 * pi * fc * m) / (pi * m);
        const double w = 0.5 - 0.5 * std::cos(2.0 * pi * static_cast<double>(n) /
                                              static_cast<double>(taps - 1));
        const double v = sinc * w;
        h[n] = static_cast<float>(v);
        sum += v;
    }
    // Normalise to the requested DC gain.
    const double scale = (sum != 0.0) ? (dc_gain / sum) : 1.0;
    for (auto& c : h) c = static_cast<float>(static_cast<double>(c) * scale);
    return h;
}

} // namespace detail

// Upsample by integer L. Emits exactly L output samples per input sample.
class Interpolator {
public:
    void configure(int factor, size_t taps_per_phase = 16) {
        l_ = factor < 1 ? 1 : factor;
        if (l_ == 1) { proto_.clear(); hist_.clear(); return; }
        taps_per_phase_ = taps_per_phase;
        const size_t taps = taps_per_phase_ * static_cast<size_t>(l_);
        // Cutoff at the input Nyquist == 0.5/L of the (high) output rate.
        // DC gain L so the interpolated passband stays unity.
        proto_ = detail::design_lowpass(taps, 0.5 / static_cast<double>(l_),
                                        static_cast<double>(l_));
        hist_.assign(taps_per_phase_, 0.0f);
        pos_ = 0;
    }

    // Appends L*n samples to `out`. `out` is cleared-append (caller owns it).
    void process(const float* in, size_t n, std::vector<float>& out) {
        if (l_ == 1) { out.insert(out.end(), in, in + n); return; }
        const size_t taps = hist_.size();   // == taps_per_phase_
        for (size_t i = 0; i < n; ++i) {
            // Circular history: newest sample lands at pos_, no O(taps) shift.
            hist_[pos_] = in[i];
            for (size_t p = 0; p < static_cast<size_t>(l_); ++p) {
                double acc = 0.0;
                for (size_t k = 0; k < taps_per_phase_; ++k) {
                    // hist index of the k-th most recent sample.
                    const size_t idx = (pos_ + taps - k) % taps;
                    acc += static_cast<double>(proto_[p + static_cast<size_t>(l_) * k]) *
                           static_cast<double>(hist_[idx]);
                }
                out.push_back(static_cast<float>(acc));
            }
            pos_ = (pos_ + 1 == taps) ? 0 : pos_ + 1;
        }
    }

    void reset() { std::fill(hist_.begin(), hist_.end(), 0.0f); pos_ = 0; }

private:
    int                l_              = 1;
    size_t             taps_per_phase_ = 0;
    size_t             pos_            = 0;   // circular head into hist_
    std::vector<float> proto_;
    std::vector<float> hist_;
};

// Downsample by integer M. Consumes M input samples per output sample;
// requires the input block length to be a multiple of M.
class Decimator {
public:
    void configure(int factor, size_t taps_per_phase = 16) {
        m_ = factor < 1 ? 1 : factor;
        if (m_ == 1) { proto_.clear(); hist_.clear(); return; }
        const size_t taps = taps_per_phase * static_cast<size_t>(m_) | 1u; // odd
        // Cutoff at the output Nyquist == 0.5/M of the (high) input rate.
        proto_ = detail::design_lowpass(taps, 0.5 / static_cast<double>(m_), 1.0);
        hist_.assign(taps, 0.0f);
        phase_ = 0;
        pos_   = 0;
    }

    void process(const float* in, size_t n, std::vector<float>& out) {
        if (m_ == 1) { out.insert(out.end(), in, in + n); return; }
        const size_t taps = hist_.size();
        for (size_t i = 0; i < n; ++i) {
            // Circular history: newest sample lands at pos_, no O(taps) shift.
            const size_t cur = pos_;
            hist_[pos_] = in[i];
            pos_ = (pos_ + 1 == taps) ? 0 : pos_ + 1;
            if (++phase_ == m_) {
                phase_ = 0;
                double acc = 0.0;
                for (size_t k = 0; k < proto_.size(); ++k) {
                    const size_t idx = (cur + taps - k) % taps;
                    acc += static_cast<double>(proto_[k]) *
                           static_cast<double>(hist_[idx]);
                }
                out.push_back(static_cast<float>(acc));
            }
        }
    }

    void reset() { std::fill(hist_.begin(), hist_.end(), 0.0f); phase_ = 0; pos_ = 0; }

private:
    int                m_     = 1;
    int                phase_ = 0;
    size_t             pos_   = 0;   // circular head into hist_
    std::vector<float> proto_;
    std::vector<float> hist_;
};

} // namespace auris

#endif // AURIS_PREPROCESS_RESAMPLER_HPP
