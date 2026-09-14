// src/detect/sound_classifier.hpp
//
// YAMNet semantic sound-event classifier. Buffers ~3 s of audio at the
// engine's sample rate, runs ONNX Runtime inference periodically, and
// reports the top-1 AudioSet class index + probability. The worker uses
// the result to emit AURIS_EVT_SOUND_CLASS events tagged with the class.
//
// Compiled only when AURIS_HAVE_SOUND_CLASS is defined (reuses the ONNX
// Runtime path established for Silero VAD).

#pragma once

#include "auris/auris.h"

#include <cstdint>
#include <memory>

namespace auris {

struct ClassificationResult {
    bool     ready       = false;   // false = not enough audio buffered yet
    int      class_id    = -1;
    float    confidence  = 0.0f;
    bool     ran         = false;   // an inference executed on this call
    bool     skipped     = false;   // an inference was due but gated
};

// Per-frame steering from the engine. `quiet` is the silence gate's verdict
// for this frame; an inference whose whole window was quiet is skipped.
// `cadence_multiplier` / `inhibit` come from adaptive degradation.
struct FeedHints {
    bool     quiet              = false;
    uint32_t cadence_multiplier = 1;
    bool     inhibit            = false;
};

class SoundClassifier {
public:
    virtual ~SoundClassifier() = default;

    virtual auris_status_t init(const auris_config_t& cfg) = 0;

    // Feed one frame of mono int16. Returns ready=true only when a fresh
    // inference completed on this call (typically every Nth frame).
    virtual ClassificationResult feed(const int16_t* frame, uint32_t n,
                                      const FeedHints& hints = FeedHints{}) = 0;
};

// Factory. Returns nullptr if the build does not have AURIS_HAVE_SOUND_CLASS.
std::unique_ptr<SoundClassifier> make_sound_classifier();

} // namespace auris
