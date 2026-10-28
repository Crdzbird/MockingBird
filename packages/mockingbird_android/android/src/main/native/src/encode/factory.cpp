// Maps a codec enum to its encoder; nullptr means the codec is not built in.
#include "encode/encoder.hpp"

namespace mockingbird {
std::unique_ptr<Encoder> make_encoder(mockingbird_codec_t codec) {
    switch (codec) {
        case MOCKINGBIRD_CODEC_PCM_S16LE: return make_passthrough_encoder();
        case MOCKINGBIRD_CODEC_OPUS:      return make_opus_encoder();
        case MOCKINGBIRD_CODEC_FLAC:      return nullptr;
    }
    return nullptr;
}

}
