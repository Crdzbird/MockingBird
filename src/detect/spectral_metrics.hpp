// The three per-frame statistics the anomaly gate scores.
#pragma once

namespace mockingbird {

struct SpectralMetrics {
    float rms      = 0.0f;
    float flux     = 0.0f;
    float centroid = 0.0f;
};

} // namespace mockingbird
