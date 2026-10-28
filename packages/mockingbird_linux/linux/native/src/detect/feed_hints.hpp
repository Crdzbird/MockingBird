// Per-frame steering for the classifier from the silence gate and the
// degradation policy.
#pragma once

#include <cstdint>

namespace mockingbird {
struct FeedHints {
    bool     quiet              = false;
    uint32_t cadence_multiplier = 1;
    bool     inhibit            = false;
};

}
