// Maps a codec enum to its encoder; nullptr means the codec is not built in.
#include "encode/encoder.hpp"

namespace auris {

std::unique_ptr<Encoder> make_encoder(auris_codec_t codec) {
    switch (codec) {
        case AURIS_CODEC_PCM_S16LE: return make_passthrough_encoder();
        case AURIS_CODEC_OPUS:      return make_opus_encoder();
        case AURIS_CODEC_FLAC:      return nullptr;
    }
    return nullptr;
}

} // namespace auris
