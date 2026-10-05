// RNNoise runs at 48 kHz on 480-sample frames, so each engine frame is
// upsampled, denoised in 480-sample chunks and downsampled again. One
// DenoiseState and both resamplers persist across frames.
#include "preprocess/preprocessor.hpp"

#ifdef MOCKINGBIRD_HAVE_RNNOISE

#include "preprocess/resampler.hpp"

extern "C" {
#include "rnnoise.h"
}

#include <algorithm>
#include <cmath>
#include <vector>

namespace mockingbird {

namespace {

constexpr int    kRnnoiseRate  = 48000;
constexpr size_t kRnnoiseFrame = 480;

class RnnoisePreprocessor final : public Preprocessor {
public:
    ~RnnoisePreprocessor() override {
        if (st_ != nullptr) rnnoise_destroy(st_);
    }

    mockingbird_status_t init(const mockingbird_config_t& cfg) override {
        if (st_ != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
        if (cfg.sample_rate_hz == 0 || (kRnnoiseRate % cfg.sample_rate_hz) != 0) {
            return MOCKINGBIRD_ERR_UNSUPPORTED;
        }
        const int factor = static_cast<int>(kRnnoiseRate / cfg.sample_rate_hz);
        up_.configure(factor);
        down_.configure(factor);
        st_ = rnnoise_create(nullptr);
        return st_ == nullptr ? MOCKINGBIRD_ERR_NO_MEMORY : MOCKINGBIRD_OK;
    }

    void process(int16_t* frame, uint32_t n) override {
        if (st_ == nullptr || frame == nullptr || n == 0) return;

        in_.resize(n);
        for (uint32_t i = 0; i < n; ++i) in_[i] = static_cast<float>(frame[i]);

        up_buf_.clear();
        up_.process(in_.data(), n, up_buf_);
        if (up_buf_.empty() || (up_buf_.size() % kRnnoiseFrame) != 0) return;

        for (size_t off = 0; off + kRnnoiseFrame <= up_buf_.size(); off += kRnnoiseFrame) {
            rnnoise_process_frame(st_, up_buf_.data() + off, up_buf_.data() + off);
        }

        down_buf_.clear();
        down_.process(up_buf_.data(), up_buf_.size(), down_buf_);

        const size_t m = std::min<size_t>(down_buf_.size(), n);
        for (size_t i = 0; i < m; ++i) {
            const float v = std::clamp(down_buf_[i], -32768.0f, 32767.0f);
            frame[i] = static_cast<int16_t>(std::lround(v));
        }
    }

private:
    DenoiseState*      st_ = nullptr;
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

} // namespace mockingbird

#else

namespace mockingbird {
std::unique_ptr<Preprocessor> make_rnnoise_preprocessor() { return nullptr; }
} // namespace mockingbird

#endif
