// Fast VAD over libfvad. The engine owns hangover and segmentation; libfvad
// keeps its own decision state.
#include "detect/detector.hpp"

extern "C" {
#include "fvad.h"
}

#include <cstdint>

namespace mockingbird {
namespace {
class VadDetector final : public Detector {
public:
    VadDetector() = default;

    ~VadDetector() override {
        if (fvad_ != nullptr) fvad_free(fvad_);
    }

    mockingbird_status_t init(const mockingbird_config_t& cfg) override {
        if (fvad_ != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
        fvad_ = fvad_new();
        if (fvad_ == nullptr) return MOCKINGBIRD_ERR_NO_MEMORY;
        if (fvad_set_sample_rate(fvad_, static_cast<int>(cfg.sample_rate_hz)) != 0) {
            return MOCKINGBIRD_ERR_UNSUPPORTED;
        }
        if (fvad_set_mode(fvad_, cfg.vad_aggressiveness) != 0) return MOCKINGBIRD_ERR_INVALID_ARG;
        frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
        return MOCKINGBIRD_OK;
    }

    DetectionResult process(const int16_t* frame, uint32_t frame_count) override {
        DetectionResult r;
        if (fvad_ == nullptr || frame == nullptr || frame_count != frame_samples_) return r;
        r.active     = fvad_process(fvad_, frame, frame_count) == 1;
        r.confidence = r.active ? 1.0f : 0.0f;
        return r;
    }

    void retune(const mockingbird_config_t& cfg) override {
        if (fvad_ == nullptr) return;
        if (cfg.vad_aggressiveness >= 0 && cfg.vad_aggressiveness <= 3) {
            fvad_set_mode(fvad_, cfg.vad_aggressiveness);
        }
    }

private:
    Fvad*    fvad_          = nullptr;
    uint32_t frame_samples_ = 0;
};

}

std::unique_ptr<Detector> make_vad(mockingbird_vad_tier_t tier) {
    switch (tier) {
        case MOCKINGBIRD_VAD_FAST:   return std::make_unique<VadDetector>();
        case MOCKINGBIRD_VAD_NEURAL: return make_neural_vad();
    }
    return nullptr;
}

}
