// The monotonic clock every timestamp in the engine derives from.
#pragma once

#include <chrono>
#include <cstdint>

namespace mockingbird {

inline uint64_t monotonic_now_us() noexcept {
    using clock = std::chrono::steady_clock;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            clock::now().time_since_epoch()).count());
}

} // namespace mockingbird
