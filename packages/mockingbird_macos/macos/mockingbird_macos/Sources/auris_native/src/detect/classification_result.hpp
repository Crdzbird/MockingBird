// The outcome of feeding one frame to the sound classifier.
#pragma once

namespace auris {

struct ClassificationResult {
    bool  ready      = false;
    int   class_id   = -1;
    float confidence = 0.0f;
    bool  ran        = false;
    bool  skipped    = false;
};

} // namespace auris
