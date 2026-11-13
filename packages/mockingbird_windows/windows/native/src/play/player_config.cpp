// Player config checks and the packet length rules push applies. Every bound
// caps an allocation or a copy made from caller input.
#include "play/player.hpp"

#include "config_validation.hpp"
#include "decode/opus_frame_decoder.hpp"

namespace mockingbird {
namespace {
bool valid_backend(int b) {
    return b == MOCKINGBIRD_BACKEND_AUTO || b == MOCKINGBIRD_BACKEND_MINIAUDIO ||
           b == MOCKINGBIRD_BACKEND_OBOE || b == MOCKINGBIRD_BACKEND_COREAUDIO ||
           b == MOCKINGBIRD_BACKEND_HOST_PCM;
}

bool valid_rate(uint32_t hz) {
    return hz == 8000 || hz == 12000 || hz == 16000 || hz == 24000 || hz == 48000;
}

}

mockingbird_status_t validate_player_config(const mockingbird_player_config_t& c) {
    if (!valid_backend(enum_as_int(c.backend))) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (!valid_rate(c.sample_rate_hz)) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (c.channels != 1 && c.channels != 2) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (c.target_delay_ms > 2000u) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (c.max_delay_ms < c.target_delay_ms || c.max_delay_ms > 10000u) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (c.conceal_max_ms > 1000u) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (c.queue_packets < 4u || c.queue_packets > 4096u) return MOCKINGBIRD_ERR_INVALID_ARG;
    for (uint32_t r : c._reserved) {
        if (r != 0) return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    return MOCKINGBIRD_OK;
}

uint32_t packet_frames_for(const mockingbird_player_config_t& c, int kind, const uint8_t* data,
                           uint32_t size, mockingbird_status_t& status) {
    status = MOCKINGBIRD_ERR_INVALID_ARG;
    if (kind == MOCKINGBIRD_CODEC_FLAC || kind == MOCKINGBIRD_CODEC_OGG_OPUS) {
        status = MOCKINGBIRD_ERR_UNSUPPORTED;
        return 0;
    }
    if (kind == MOCKINGBIRD_CODEC_OPUS && !OpusFrameDecoder::valid_rate(c.sample_rate_hz)) {
        status = MOCKINGBIRD_ERR_UNSUPPORTED;
        return 0;
    }
    if (data == nullptr || size == 0 || size > kPlayerMaxPacketBytes) return 0;
    const uint32_t cap = OpusFrameDecoder::max_frames_for(c.sample_rate_hz);
    uint32_t frames = 0;
    if (kind == MOCKINGBIRD_CODEC_PCM_S16LE) {
        const uint32_t frame_bytes = 2u * c.channels;
        if (size % frame_bytes != 0) return 0;
        frames = size / frame_bytes;
    } else if (kind == MOCKINGBIRD_CODEC_OPUS) {
        const int n = OpusFrameDecoder::frames_in(data, size, c.sample_rate_hz);
        if (n <= 0) return 0;
        frames = static_cast<uint32_t>(n);
    }
    if (frames == 0 || frames > cap) return 0;
    status = MOCKINGBIRD_OK;
    return frames;
}

}
