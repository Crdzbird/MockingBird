// The live-tunable subset of the configuration as the worker sees it,
// refreshed between frames whenever the host bumps the tune generation.
#pragma once

#include "auris/auris.h"
#include "degrade.hpp"

#include <cstdint>

struct auris_engine;

namespace auris {

class Detector;

class Tunables {
public:
    void load(const auris_engine* e);
    bool refresh(auris_engine* e, Detector* vad, Detector* anomaly);

    bool     vad_enabled          = false;
    bool     anomaly_enabled      = false;
    bool     sound_class_enabled  = false;
    bool     adaptive_on          = false;
    float    classifier_threshold = 0.4f;
    float    silence_gate_dbfs    = -60.0f;
    uint32_t hangover_frames      = 0;
    DegradationPolicy degrade;

private:
    void apply(const auris_config_t& c);

    uint32_t frame_ms_        = 20;
    uint64_t seen_generation_ = 0;
};

} // namespace auris
