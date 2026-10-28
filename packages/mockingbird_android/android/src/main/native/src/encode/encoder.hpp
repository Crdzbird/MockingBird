// Per-frame encoder owned by the worker thread. encode() returns bytes
// written, 0 for a deliberately empty frame, or a negative value on failure.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace mockingbird {
class Encoder {
public:
    virtual ~Encoder() = default;

    virtual mockingbird_status_t init(const mockingbird_config_t& cfg) = 0;
    virtual mockingbird_codec_t codec() const = 0;
    virtual size_t max_packet_bytes() const = 0;
    virtual int encode(const int16_t* in_samples, uint32_t n, uint8_t* out_buf,
                       uint32_t out_cap) = 0;
};

std::unique_ptr<Encoder> make_encoder(mockingbird_codec_t codec);
std::unique_ptr<Encoder> make_passthrough_encoder();
std::unique_ptr<Encoder> make_opus_encoder();

}
