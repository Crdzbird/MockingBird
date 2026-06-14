// src/encode/opus_encoder.cpp
//
// libopus encoder, compiled only when AURIS_HAVE_OPUS is defined (i.e.
// when the build system has linked opus into auris). When the macro is
// absent, make_opus_encoder returns nullptr so make_encoder() can surface
// AURIS_ERR_UNSUPPORTED at validate_config time.
//
// // VERIFY  opus_encode max_data_bytes: per RFC 6716 §3.4 a frame should
// never need more than 1275 bytes. We allocate 4000 to leave headroom
// for the rare cases the encoder asks for more buffer at very high
// bitrates / large frames.

#include "encoder.hpp"

#ifdef AURIS_HAVE_OPUS

extern "C" {
#include <opus.h>
}

#include <cstdint>

namespace auris {

namespace {

constexpr size_t kOpusMaxPacketBytes = 4000;

class LibOpusEncoder final : public Encoder {
public:
    LibOpusEncoder() = default;

    ~LibOpusEncoder() override {
        if (enc_ != nullptr) {
            opus_encoder_destroy(enc_);
            enc_ = nullptr;
        }
    }

    auris_status_t init(const auris_config_t& cfg) override {
        if (enc_ != nullptr) return AURIS_ERR_INVALID_STATE;

        // // VERIFY  Opus accepts {8, 12, 16, 24, 48} kHz. validate_config
        // already restricts sample_rate to {8/16/32/48}; 32 kHz is NOT
        // an Opus rate, so we reject it here for the OPUS codec specifically.
        if (cfg.sample_rate_hz != 8000  &&
            cfg.sample_rate_hz != 16000 &&
            cfg.sample_rate_hz != 24000 &&
            cfg.sample_rate_hz != 48000) {
            return AURIS_ERR_UNSUPPORTED;
        }

        int err = 0;
        enc_ = opus_encoder_create(static_cast<opus_int32>(cfg.sample_rate_hz),
                                   cfg.channels,
                                   OPUS_APPLICATION_VOIP,
                                   &err);
        if (enc_ == nullptr || err != OPUS_OK) {
            return AURIS_ERR_BACKEND;
        }

        opus_encoder_ctl(enc_, OPUS_SET_BITRATE(static_cast<opus_int32>(cfg.opus_bitrate_bps)));
        opus_encoder_ctl(enc_, OPUS_SET_VBR(cfg.opus_vbr));
        opus_encoder_ctl(enc_, OPUS_SET_COMPLEXITY(cfg.opus_complexity));

        frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
        return AURIS_OK;
    }

    auris_codec_t codec() const override { return AURIS_CODEC_OPUS; }
    size_t        max_packet_bytes() const override { return kOpusMaxPacketBytes; }

    int encode(const int16_t* in_samples,
               uint32_t       n,
               uint8_t*       out_buf,
               uint32_t       out_cap) override {
        if (enc_ == nullptr || n != frame_samples_) return -1;
        const int r = opus_encode(enc_,
                                  in_samples,
                                  static_cast<int>(n),
                                  out_buf,
                                  static_cast<opus_int32>(out_cap));
        return r;     // > 0 = bytes; < 0 = OPUS_* error
    }

private:
    ::OpusEncoder* enc_           = nullptr;
    uint32_t       frame_samples_ = 0;
};

} // namespace

std::unique_ptr<Encoder> make_opus_encoder() {
    return std::make_unique<LibOpusEncoder>();
}

} // namespace auris

#else   // !AURIS_HAVE_OPUS

namespace auris {
std::unique_ptr<Encoder> make_opus_encoder() { return nullptr; }
} // namespace auris

#endif
