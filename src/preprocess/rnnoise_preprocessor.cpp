// src/preprocess/rnnoise_preprocessor.cpp
//
// RNNoise (Xiph, BSD-3) recurrent noise suppression. RNNoise is fixed at
// 48 kHz with 480-sample (10 ms) frames, while the Auris pipeline runs at
// 8 k / 16 k. We bridge it frame-locally: upsample the int16 frame to 48 kHz,
// denoise in 480-sample chunks, then downsample back. The working signal is
// band-limited to its own Nyquist, so RNNoise's upper 48 kHz bands are inert
// and only the in-band noise is cleaned.
//
// One DenoiseState spans the whole stream (RNNoise is recurrent), and the
// resamplers carry FIR history, so block-by-block processing stays continuous.

#include "preprocess/preprocessor.hpp"

#ifdef AURIS_HAVE_RNNOISE

#include "preprocess/resampler.hpp"

extern "C" {
#include "rnnoise.h"
}

#include <algorithm>
#include <cmath>
#include <vector>

namespace auris {

namespace {

constexpr int    kRnnoiseRate  = 48000;
constexpr size_t kRnnoiseFrame = 480;   // 10 ms @ 48 kHz

class RnnoisePreprocessor final : public Preprocessor {
public:
    ~RnnoisePreprocessor() override {
        if (st_ != nullptr) rnnoise_destroy(st_);
    }

    auris_status_t init(const auris_config_t& cfg) override {
        if (st_ != nullptr) return AURIS_ERR_INVALID_STATE;
        // Need an integer 48 kHz <-> working-rate ratio (8 k, 16 k, 48 k).
        if (cfg.sample_rate_hz == 0 ||
            (kRnnoiseRate % cfg.sample_rate_hz) != 0) {
            return AURIS_ERR_UNSUPPORTED;
        }
        factor_ = static_cast<int>(kRnnoiseRate / cfg.sample_rate_hz);
        up_.configure(factor_);
        down_.configure(factor_);

        st_ = rnnoise_create(nullptr);   // built-in model
        if (st_ == nullptr) return AURIS_ERR_NO_MEMORY;
        return AURIS_OK;
    }

    void process(int16_t* frame, uint32_t n) override {
        if (st_ == nullptr || frame == nullptr || n == 0) return;

        // int16 -> int16-range float (RNNoise works in counts, not [-1, 1]).
        in_.resize(n);
        for (uint32_t i = 0; i < n; ++i) {
            in_[i] = static_cast<float>(frame[i]);
        }

        up_buf_.clear();
        up_.process(in_.data(), n, up_buf_);     // n * factor samples @ 48 kHz
        // Frame lengths are chosen so this is always a whole number of
        // RNNoise frames (frame_ms * 48 is a multiple of 480). Guard anyway.
        if (up_buf_.empty() || (up_buf_.size() % kRnnoiseFrame) != 0) return;

        for (size_t off = 0; off + kRnnoiseFrame <= up_buf_.size();
             off += kRnnoiseFrame) {
            // In-place: RNNoise consumes `in` before writing `out`.
            rnnoise_process_frame(st_, up_buf_.data() + off, up_buf_.data() + off);
        }

        down_buf_.clear();
        down_.process(up_buf_.data(), up_buf_.size(), down_buf_);  // back to n

        const size_t m = std::min<size_t>(down_buf_.size(), n);
        for (size_t i = 0; i < m; ++i) {
            float v = down_buf_[i];
            v = std::min(32767.0f, std::max(-32768.0f, v));
            frame[i] = static_cast<int16_t>(std::lround(v));
        }
    }

private:
    DenoiseState*      st_     = nullptr;
    int                factor_ = 1;
    Interpolator       up_;
    Decimator          down_;
    std::vector<float> in_;
    std::vector<float> up_buf_;
    std::vector<float> down_buf_;
};

} // namespace

std::unique_ptr<Preprocessor> make_rnnoise_preprocessor() {
    return std::make_unique<RnnoisePreprocessor>();
}

} // namespace auris

#else   // !AURIS_HAVE_RNNOISE

namespace auris {
std::unique_ptr<Preprocessor> make_rnnoise_preprocessor() { return nullptr; }
} // namespace auris

#endif
