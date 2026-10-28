// One frame's verdict from a detector. On the frame where speech is first
// confirmed, onset_lead_us says how long before that frame the speech began.
#pragma once

#include <cstdint>

namespace mockingbird {
struct DetectionResult {
    bool     active      = false;
    uint32_t reason_mask = 0;
    float    confidence  = 0.0f;
    uint64_t onset_lead_us = 0;
};

}
