// One frame's verdict from a detector.
#pragma once

#include <cstdint>

namespace mockingbird {
struct DetectionResult {
    bool     active      = false;
    uint32_t reason_mask = 0;
    float    confidence  = 0.0f;
};

}
