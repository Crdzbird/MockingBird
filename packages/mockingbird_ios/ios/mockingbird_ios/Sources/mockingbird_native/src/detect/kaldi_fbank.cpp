// Window and mel tables are built once; each frame is cut from the signal,
// conditioned, transformed and folded into the mel bins. The mel scale is
// Kaldi's, 1127 ln(1 + f / 700), and log energies are floored at FLT_EPSILON.
#include "detect/kaldi_fbank.hpp"

#include <cfloat>
#include <cmath>
#include <memory>

namespace mockingbird {
namespace {
float mel(float hz) { return 1127.0f * std::log(1.0f + hz / 700.0f); }

struct AlignedFree {
    void operator()(float* p) const { pffft_aligned_free(p); }
};
using Aligned = std::unique_ptr<float, AlignedFree>;

Aligned aligned(size_t n) { return Aligned(static_cast<float*>(pffft_aligned_malloc(n * sizeof(float)))); }

}

KaldiFbank::KaldiFbank() : setup_(pffft_new_setup(kFft, PFFFT_REAL)), window_(kLength), bins_(kBins) {
    for (size_t i = 0; i < kLength; ++i) {
        const double c = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * static_cast<double>(i) / (kLength - 1));
        window_[i]     = static_cast<float>(std::pow(c, 0.85));
    }
    const float lo = mel(20.0f), delta = (mel(7600.0f) - lo) / (kBins + 1);
    for (size_t b = 0; b < kBins; ++b) {
        const float left = lo + static_cast<float>(b) * delta, centre = left + delta, right = centre + delta;
        for (size_t i = 0; i < kFft / 2; ++i) {
            const float m = mel(16000.0f * static_cast<float>(i) / kFft);
            if (m <= left || m >= right) continue;
            if (bins_[b].weights.empty()) bins_[b].first = i;
            bins_[b].weights.push_back(m <= centre ? (m - left) / (centre - left) : (right - m) / (right - centre));
        }
    }
}

KaldiFbank::~KaldiFbank() {
    if (setup_ != nullptr) pffft_destroy_setup(setup_);
}

void KaldiFbank::frame_into(const int16_t* pcm, size_t n, size_t f, float* padded) const {
    const auto len   = static_cast<int64_t>(n);
    const int64_t at = static_cast<int64_t>(f * kShift + kShift / 2) - static_cast<int64_t>(kLength / 2);
    float sum = 0.0f;
    for (size_t s = 0; s < kLength; ++s) {
        int64_t i = at + static_cast<int64_t>(s);
        while (i < 0 || i >= len) i = i < 0 ? -i - 1 : 2 * len - 1 - i;
        padded[s] = static_cast<float>(pcm[i]) / 32768.0f;
        sum += padded[s];
    }
    const float mean = sum / kLength;
    for (size_t s = 0; s < kLength; ++s) padded[s] -= mean;
    for (size_t s = kLength - 1; s > 0; --s) padded[s] -= 0.97f * padded[s - 1];
    padded[0] -= 0.97f * padded[0];
    for (size_t s = 0; s < kLength; ++s) padded[s] *= window_[s];
    for (size_t s = kLength; s < kFft; ++s) padded[s] = 0.0f;
}

std::vector<float> KaldiFbank::compute(const int16_t* pcm, size_t n) const {
    std::vector<float> out;
    if (setup_ == nullptr || pcm == nullptr || n == 0) return out;
    const size_t frames = (n + kShift / 2) / kShift;
    Aligned padded = aligned(kFft), spectrum = aligned(kFft), work = aligned(kFft);
    std::vector<float> power(kFft / 2);
    out.resize(frames * kBins);
    for (size_t f = 0; f < frames; ++f) {
        frame_into(pcm, n, f, padded.get());
        pffft_transform_ordered(setup_, padded.get(), spectrum.get(), work.get(), PFFFT_FORWARD);
        const float* s = spectrum.get();
        power[0] = s[0] * s[0];
        for (size_t k = 1; k < kFft / 2; ++k) power[k] = s[2 * k] * s[2 * k] + s[2 * k + 1] * s[2 * k + 1];
        for (size_t b = 0; b < kBins; ++b) {
            float e = 0.0f;
            for (size_t w = 0; w < bins_[b].weights.size(); ++w) e += bins_[b].weights[w] * power[bins_[b].first + w];
            out[f * kBins + b] = std::log(std::fmax(e, FLT_EPSILON));
        }
    }
    return out;
}

}
