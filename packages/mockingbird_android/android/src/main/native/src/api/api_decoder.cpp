// mockingbird_decoder_*: the standalone Opus decoder behind an opaque handle.
#include "mockingbird/mockingbird.h"
#include "decode/opus_frame_decoder.hpp"

#include <new>

struct mockingbird_decoder {
    mockingbird::OpusFrameDecoder opus;
};

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_decoder_create(uint32_t sample_rate_hz, uint16_t channels,
                                                                mockingbird_decoder_t** out_decoder) {
    if (out_decoder == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    *out_decoder = nullptr;
    auto* d = new (std::nothrow) mockingbird_decoder();
    if (d == nullptr) return MOCKINGBIRD_ERR_NO_MEMORY;
    const mockingbird_status_t s = d->opus.init(sample_rate_hz, channels);
    if (s != MOCKINGBIRD_OK) {
        delete d;
        return s;
    }
    *out_decoder = d;
    return MOCKINGBIRD_OK;
}

MOCKINGBIRD_API void mockingbird_decoder_destroy(mockingbird_decoder_t* decoder) { delete decoder; }

MOCKINGBIRD_API mockingbird_status_t mockingbird_decoder_decode(mockingbird_decoder_t* decoder,
                                                                const uint8_t* data, uint32_t size,
                                                                int16_t* out, uint32_t max_frames,
                                                                uint32_t* out_frames) {
    if (out_frames != nullptr) *out_frames = 0;
    if (decoder == nullptr || out_frames == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    return decoder->opus.decode(data, size, out, max_frames, *out_frames);
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_decoder_conceal(mockingbird_decoder_t* decoder,
                                                                 uint32_t frames, int16_t* out,
                                                                 uint32_t max_frames,
                                                                 uint32_t* out_frames) {
    if (out_frames != nullptr) *out_frames = 0;
    if (decoder == nullptr || out_frames == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    return decoder->opus.conceal(frames, out, max_frames, *out_frames);
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_decoder_reset(mockingbird_decoder_t* decoder) {
    if (decoder == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    decoder->opus.reset();
    return MOCKINGBIRD_OK;
}

}
