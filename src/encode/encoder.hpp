// src/encode/encoder.hpp
//
// Abstract per-frame encoder. The worker pulls one PCM frame at a time off
// the ring and asks the encoder to turn it into bytes for the packet pool.
//
// Two implementations:
//   PassthroughEncoder — copies int16 LE bytes unchanged.
//   LibOpusEncoder     — wraps libopus encode at the engine's bitrate.
//
// The factory's choice is fixed at engine create time; the encoder lives
// on the worker thread and is never re-entered concurrently.

#pragma once

#include "auris/auris.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace auris {

class Encoder {
public:
    virtual ~Encoder() = default;

    virtual auris_status_t init(const auris_config_t& cfg) = 0;

    // Codec emitted by this encoder; copied into auris_packet.codec.
    virtual auris_codec_t codec() const = 0;

    // Worst-case bytes a single encoded frame may produce. The packet pool
    // sizes its slots to this so the worker never overruns.
    virtual size_t max_packet_bytes() const = 0;

    // Encode one frame. Returns bytes written on success, 0 if the encoder
    // chose to emit nothing for this frame (silence DTX), or a negative
    // value on encoder failure (worker treats as a packet drop).
    virtual int encode(const int16_t* in_samples,
                       uint32_t       n,
                       uint8_t*       out_buf,
                       uint32_t       out_cap) = 0;
};

// Factory. Returns nullptr if the codec is not compiled into this build
// (e.g. AURIS_CODEC_OPUS without AURIS_HAVE_OPUS).
std::unique_ptr<Encoder> make_encoder(auris_codec_t codec);

} // namespace auris
