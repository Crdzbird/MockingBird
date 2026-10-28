// What the two frame detectors said about one frame, and whether each ran.
#pragma once

#include "detect/detection_result.hpp"

namespace mockingbird {
struct DetectionPair {
    DetectionResult vad;
    DetectionResult anomaly;
    bool            vad_on     = false;
    bool            anomaly_on = false;

    bool running() const { return vad_on || anomaly_on; }
};

}
