// PCM push for the WebAssembly shim: the capture timestamp arrives as a
// double because a 64-bit integer would cross into JavaScript as a BigInt.
#ifdef __EMSCRIPTEN__

#include "mockingbird/mockingbird.h"

#include <emscripten/emscripten.h>

#include <cstdint>

extern "C" {
EMSCRIPTEN_KEEPALIVE int mockingbird_web_push_pcm(mockingbird_engine_t* e, const int16_t* samples,
                                            uint32_t frame_count, double capture_ts_us) {
    return static_cast<int>(mockingbird_push_pcm(e, samples, frame_count,
                                           static_cast<uint64_t>(capture_ts_us)));
}

}

#endif
