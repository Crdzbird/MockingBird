// Per-frame detector interface and the factories for the two detectors.
#pragma once

#include "auris/auris.h"
#include "detect/detection_result.hpp"

#include <cstdint>
#include <memory>

namespace auris {

class Detector {
public:
    virtual ~Detector() = default;

    virtual auris_status_t init(const auris_config_t& cfg) = 0;

    virtual DetectionResult process(const int16_t* frame, uint32_t frame_count) = 0;

    virtual void retune(const auris_config_t&) {}
};

std::unique_ptr<Detector> make_vad(auris_vad_tier_t tier);

std::unique_ptr<Detector> make_neural_vad();

std::unique_ptr<Detector> make_anomaly();

} // namespace auris
