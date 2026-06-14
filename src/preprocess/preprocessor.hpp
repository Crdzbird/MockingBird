// src/preprocess/preprocessor.hpp
//
// Optional front-end that cleans the PCM stream before it reaches the
// detectors and the encoder. v1 ships a single implementation (RNNoise);
// the interface keeps the worker agnostic so RNNoise can be swapped or
// stacked later.

#ifndef AURIS_PREPROCESS_PREPROCESSOR_HPP
#define AURIS_PREPROCESS_PREPROCESSOR_HPP

#include "auris/auris.h"

#include <cstdint>
#include <memory>

namespace auris {

class Preprocessor {
public:
    virtual ~Preprocessor() = default;

    // Prepare for cfg.sample_rate_hz / cfg.frame_ms. Returns AURIS_ERR_*
    // (e.g. AURIS_ERR_UNSUPPORTED) when the configuration can't be served.
    virtual auris_status_t init(const auris_config_t& cfg) = 0;

    // In-place clean one frame of `n` int16 samples at the configured rate.
    // Must be alloc-light and never throw (runs on the worker thread, T2).
    virtual void process(int16_t* frame, uint32_t n) = 0;
};

// RNNoise-backed preprocessor. Returns nullptr when the build was compiled
// without AURIS_HAVE_RNNOISE so the engine can surface AURIS_ERR_UNSUPPORTED.
std::unique_ptr<Preprocessor> make_rnnoise_preprocessor();

} // namespace auris

#endif // AURIS_PREPROCESS_PREPROCESSOR_HPP
