// The live-tunable subset of the configuration as the worker sees it,
// refreshed between frames whenever the host bumps the tune generation.
#pragma once

#include "mockingbird/mockingbird.h"
#include "degrade.hpp"

#include <cstdint>

struct mockingbird_engine;

namespace mockingbird {
class Detector;

class Tunables {
public:
    void load(const mockingbird_engine* e);
    bool refresh(mockingbird_engine* e, Detector* vad, Detector* anomaly);

    bool     vad_enabled          = false;
    bool     anomaly_enabled      = false;
    bool     sound_class_enabled  = false;
    bool     adaptive_on          = false;
    float    classifier_threshold = 0.4f;
    float    silence_gate_dbfs    = -60.0f;
    uint32_t hangover_frames      = 0;
    DegradationPolicy degrade;

private:
    void apply(const mockingbird_config_t& c);

    uint32_t frame_ms_        = 20;
    uint64_t seen_generation_ = 0;
};

}
