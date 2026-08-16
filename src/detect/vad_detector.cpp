// src/detect/vad_detector.cpp
//
// FAST VAD via libfvad (WebRTC's GMM-based detector). Stateless per-frame
// from our perspective: libfvad holds its own decision state internally;
// the engine handles hangover / segment open-close around it.

#include "detector.hpp"

extern "C" {
#include "fvad.h"
}

#include <cstdint>

namespace auris {

namespace {

class VadDetector final : public Detector {
public:
    VadDetector() = default;

    ~VadDetector() override {
        if (fvad_ != nullptr) {
            fvad_free(fvad_);
            fvad_ = nullptr;
        }
    }

    auris_status_t init(const auris_config_t& cfg) override {
        if (fvad_ != nullptr) return AURIS_ERR_INVALID_STATE;

        fvad_ = fvad_new();
        if (fvad_ == nullptr) return AURIS_ERR_NO_MEMORY;

        // libfvad sample-rate: {8000, 16000, 32000, 48000} Hz. Already
        // validated against this set in engine.cpp validate_config(), but
        // re-check at the boundary so this detector is safe to construct
        // standalone.
        if (fvad_set_sample_rate(fvad_, static_cast<int>(cfg.sample_rate_hz)) != 0) {
            return AURIS_ERR_UNSUPPORTED;
        }
        // Aggressiveness 0..3 (least to most aggressive about classifying
        // borderline frames as non-speech).
        if (fvad_set_mode(fvad_, cfg.vad_aggressiveness) != 0) {
            return AURIS_ERR_INVALID_ARG;
        }

        // libfvad frame length: {10, 20, 30} ms == sample_rate * ms / 1000
        // samples. Cached so process() can fail-fast on a wrong-sized frame.
        frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
        return AURIS_OK;
    }

    DetectionResult process(const int16_t* frame, uint32_t frame_count) override {
        DetectionResult r;
        if (fvad_ == nullptr || frame == nullptr) return r;
        if (frame_count != frame_samples_)        return r;

        // fvad_process returns 1 (speech), 0 (not speech), or -1 (error).
        // Treat -1 as not speech.
        const int v = fvad_process(fvad_, frame, frame_count);
        r.active     = (v == 1);
        r.confidence = r.active ? 1.0f : 0.0f;
        return r;
    }

    // Aggressiveness is the only live-tunable knob; fvad_set_mode keeps the
    // detector's internal GMM state, so speech in progress is not disturbed.
    void retune(const auris_config_t& cfg) override {
        if (fvad_ == nullptr) return;
        if (cfg.vad_aggressiveness >= 0 && cfg.vad_aggressiveness <= 3) {
            fvad_set_mode(fvad_, cfg.vad_aggressiveness);
        }
    }

private:
    Fvad*    fvad_         = nullptr;
    uint32_t frame_samples_ = 0;
};

} // namespace

std::unique_ptr<Detector> make_vad(auris_vad_tier_t tier) {
    switch (tier) {
        case AURIS_VAD_FAST:
            return std::make_unique<VadDetector>();
        case AURIS_VAD_NEURAL:
            return make_neural_vad();   // nullptr if AURIS_HAVE_NEURAL_VAD off
    }
    return nullptr;
}

} // namespace auris
