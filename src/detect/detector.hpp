// src/detect/detector.hpp
//
// Abstract per-frame detection interface. Implementations consume a single
// PCM frame at the engine's configured sample-rate / frame_ms cadence and
// return whether their "signal" is currently active, optionally tagged with
// detector-specific reason bits.
//
// VAD: active == "speech this frame". reason_mask is unused.
// Anomaly: active == "anomalous this frame". reason_mask carries the
// AURIS_ANOMALY_* bits indicating which metric(s) crossed threshold.

#pragma once

#include "auris/auris.h"

#include <cstdint>
#include <memory>

namespace auris {

struct DetectionResult {
    bool     active      = false;
    uint32_t reason_mask = 0;
    float    confidence  = 0.0f;
};

class Detector {
public:
    virtual ~Detector() = default;

    // Prepare with the engine config. Returns AURIS_OK on success, or a
    // specific status if the detector cannot satisfy the requested sample
    // rate / frame size.
    virtual auris_status_t init(const auris_config_t& cfg) = 0;

    // Process one frame of mono int16 samples. Stateless from the engine's
    // perspective; each detector maintains its own internal state.
    virtual DetectionResult process(const int16_t* frame,
                                    uint32_t       frame_count) = 0;
};

// Factory for a Voice Activity Detector. AURIS_VAD_FAST → libfvad GMM;
// AURIS_VAD_NEURAL → Silero v5 via ONNX Runtime (compiled in only when
// AURIS_HAVE_NEURAL_VAD is defined). Returns nullptr for unsupported tiers.
std::unique_ptr<Detector> make_vad(auris_vad_tier_t tier);

// Internal factory exposed so the test build can swap it in.
std::unique_ptr<Detector> make_neural_vad();

// Factory for the anomaly gate (PFFFT-based adaptive RMS + spectral flux +
// spectral centroid over a rolling background). Returns nullptr if anomaly
// is unsupported in this build.
std::unique_ptr<Detector> make_anomaly();

} // namespace auris
