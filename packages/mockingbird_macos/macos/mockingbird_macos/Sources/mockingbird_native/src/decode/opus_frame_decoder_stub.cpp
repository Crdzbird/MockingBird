// The decoder in a build without libopus: every call reports UNSUPPORTED,
// so the C ABI and the player link and fail cleanly at run time.
#include "decode/opus_frame_decoder.hpp"

#ifndef MOCKINGBIRD_HAVE_OPUS

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
