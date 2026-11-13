// libopus decode and concealment. libopus itself bounds every write by the
// frame size it is given; the checks here add the C ABI's own contract on
// top: a packet longer than the caller's room is refused before decoding.
#include "decode/opus_frame_decoder.hpp"

#ifdef MOCKINGBIRD_HAVE_OPUS

extern "C" {
#include <opus.h>
}

namespace mockingbird {
OpusFrameDecoder::~OpusFrameDecoder() {
    if (dec_ != nullptr) opus_decoder_destroy(dec_);
}

bool OpusFrameDecoder::valid_rate(uint32_t hz) noexcept {
    return hz == 8000 || hz == 12000 || hz == 16000 || hz == 24000 || hz == 48000;
}

mockingbird_status_t OpusFrameDecoder::init(uint32_t sample_rate_hz, uint16_t channels) noexcept {
    if (dec_ != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (!valid_rate(sample_rate_hz) || (channels != 1 && channels != 2)) {
        return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    int err = OPUS_OK;
    dec_ = opus_decoder_create(static_cast<opus_int32>(sample_rate_hz), channels, &err);
    if (dec_ == nullptr || err != OPUS_OK) {
        dec_ = nullptr;
        return err == OPUS_ALLOC_FAIL ? MOCKINGBIRD_ERR_NO_MEMORY : MOCKINGBIRD_ERR_BACKEND;
    }
    rate_     = sample_rate_hz;
    channels_ = channels;
    return MOCKINGBIRD_OK;
}

int OpusFrameDecoder::frames_in(const uint8_t* data, uint32_t size, uint32_t hz) noexcept {
    if (data == nullptr || size == 0 || size > kMaxPacketBytes || !valid_rate(hz)) return -1;
    return opus_packet_get_nb_samples(data, static_cast<opus_int32>(size), static_cast<opus_int32>(hz));
}

int OpusFrameDecoder::packet_frames(const uint8_t* data, uint32_t size) const noexcept {
    if (dec_ == nullptr || data == nullptr || size == 0 || size > kMaxPacketBytes) return -1;
    const int n = opus_decoder_get_nb_samples(dec_, data, static_cast<opus_int32>(size));
    return n > 0 && static_cast<uint32_t>(n) <= max_frames_for(rate_) ? n : -1;
}

mockingbird_status_t OpusFrameDecoder::decode(const uint8_t* data, uint32_t size, int16_t* out,
                                              uint32_t max_frames, uint32_t& out_frames) noexcept {
    out_frames = 0;
    if (dec_ == nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (out == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    const int frames = packet_frames(data, size);
    if (frames < 0 || static_cast<uint32_t>(frames) > max_frames) return MOCKINGBIRD_ERR_INVALID_ARG;
    const int n = opus_decode(dec_, data, static_cast<opus_int32>(size), out, frames, 0);
    if (n < 0) return MOCKINGBIRD_ERR_INVALID_ARG;
    out_frames   = static_cast<uint32_t>(n);
    last_frames_ = out_frames;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t OpusFrameDecoder::conceal(uint32_t frames, int16_t* out, uint32_t max_frames,
                                               uint32_t& out_frames) noexcept {
    out_frames = 0;
    if (dec_ == nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (out == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    const uint32_t step   = rate_ / 400u;
    const uint32_t wanted = frames != 0 ? frames : (last_frames_ != 0 ? last_frames_ : rate_ / 50u);
    if (wanted % step != 0 || wanted > max_frames_for(rate_) || wanted > max_frames) {
        return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    const int n = opus_decode(dec_, nullptr, 0, out, static_cast<int>(wanted), 0);
    if (n < 0) return MOCKINGBIRD_ERR_INTERNAL;
    out_frames = static_cast<uint32_t>(n);
    return MOCKINGBIRD_OK;
}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif
void OpusFrameDecoder::reset() noexcept {
    if (dec_ != nullptr) opus_decoder_ctl(dec_, OPUS_RESET_STATE);
    last_frames_ = 0;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

}

#else

namespace mockingbird {
OpusFrameDecoder::~OpusFrameDecoder() = default;
bool OpusFrameDecoder::valid_rate(uint32_t) noexcept { return false; }
mockingbird_status_t OpusFrameDecoder::init(uint32_t, uint16_t) noexcept { return MOCKINGBIRD_ERR_UNSUPPORTED; }
int OpusFrameDecoder::packet_frames(const uint8_t*, uint32_t) const noexcept { return -1; }
int OpusFrameDecoder::frames_in(const uint8_t*, uint32_t, uint32_t) noexcept { return -1; }
mockingbird_status_t OpusFrameDecoder::decode(const uint8_t*, uint32_t, int16_t*, uint32_t,
                                              uint32_t& out_frames) noexcept {
    out_frames = 0;
    return MOCKINGBIRD_ERR_UNSUPPORTED;
}
mockingbird_status_t OpusFrameDecoder::conceal(uint32_t, int16_t*, uint32_t, uint32_t& out_frames) noexcept {
    out_frames = 0;
    return MOCKINGBIRD_ERR_UNSUPPORTED;
}
void OpusFrameDecoder::reset() noexcept {}
}

#endif
