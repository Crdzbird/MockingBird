// libopus encoder. Opus accepts 8, 12, 16, 24 and 48 kHz; RFC 6716 bounds a
// frame at 1275 bytes, and the slot size leaves headroom above that.
#include "encode/encoder.hpp"

#ifdef MOCKINGBIRD_HAVE_OPUS

extern "C" {
#include <opus.h>
}

#include <cstdint>

namespace mockingbird {

namespace {

constexpr size_t kOpusMaxPacketBytes = 4000;

bool opus_supports_rate(uint32_t hz) {
    return hz == 8000 || hz == 16000 || hz == 24000 || hz == 48000;
}

class LibOpusEncoder final : public Encoder {
public:
    ~LibOpusEncoder() override {
        if (enc_ != nullptr) opus_encoder_destroy(enc_);
    }

    mockingbird_status_t init(const mockingbird_config_t& cfg) override {
        if (enc_ != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
        if (!opus_supports_rate(cfg.sample_rate_hz)) return MOCKINGBIRD_ERR_UNSUPPORTED;

        int err = 0;
        enc_ = opus_encoder_create(static_cast<opus_int32>(cfg.sample_rate_hz), cfg.channels,
                                   OPUS_APPLICATION_VOIP, &err);
        if (enc_ == nullptr || err != OPUS_OK) return MOCKINGBIRD_ERR_BACKEND;

        apply_settings(cfg);

        frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
        return MOCKINGBIRD_OK;
    }

    mockingbird_codec_t codec() const override { return MOCKINGBIRD_CODEC_OPUS; }
    size_t max_packet_bytes() const override { return kOpusMaxPacketBytes; }

    int encode(const int16_t* in_samples, uint32_t n, uint8_t* out_buf,
               uint32_t out_cap) override {
        if (enc_ == nullptr || n != frame_samples_) return -1;
        return opus_encode(enc_, in_samples, static_cast<int>(n), out_buf,
                           static_cast<opus_int32>(out_cap));
    }

private:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
    void apply_settings(const mockingbird_config_t& cfg) {
        opus_encoder_ctl(enc_, OPUS_SET_BITRATE(static_cast<opus_int32>(cfg.opus_bitrate_bps)));
        opus_encoder_ctl(enc_, OPUS_SET_VBR(cfg.opus_vbr));
        opus_encoder_ctl(enc_, OPUS_SET_COMPLEXITY(cfg.opus_complexity));
    }
#pragma clang diagnostic pop

    ::OpusEncoder* enc_           = nullptr;
    uint32_t       frame_samples_ = 0;
};

} // namespace

std::unique_ptr<Encoder> make_opus_encoder() {
    return std::make_unique<LibOpusEncoder>();
}

} // namespace mockingbird

#else

namespace mockingbird {
std::unique_ptr<Encoder> make_opus_encoder() { return nullptr; }
} // namespace mockingbird

#endif
