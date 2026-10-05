// Per-frame detector interface and the factories for the two detectors.
#pragma once

#include "mockingbird/mockingbird.h"
#include "detect/detection_result.hpp"

#include <cstdint>
#include <memory>

namespace mockingbird {

class Detector {
public:
    virtual ~Detector() = default;

    virtual mockingbird_status_t init(const mockingbird_config_t& cfg) = 0;

    virtual DetectionResult process(const int16_t* frame, uint32_t frame_count) = 0;

    virtual void retune(const mockingbird_config_t&) {}
};

std::unique_ptr<Detector> make_vad(mockingbird_vad_tier_t tier);

std::unique_ptr<Detector> make_neural_vad();

std::unique_ptr<Detector> make_anomaly();

} // namespace mockingbird
