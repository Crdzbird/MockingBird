// src/encode/factory.cpp

#include "encoder.hpp"

namespace auris {

std::unique_ptr<Encoder> make_passthrough_encoder();
std::unique_ptr<Encoder> make_opus_encoder();

std::unique_ptr<Encoder> make_encoder(auris_codec_t codec) {
    switch (codec) {
        case AURIS_CODEC_PCM_S16LE: return make_passthrough_encoder();
        case AURIS_CODEC_OPUS:      return make_opus_encoder();
        case AURIS_CODEC_FLAC:      return nullptr;  // // ASSUMES  step 4.5
    }
    return nullptr;
}

} // namespace auris
