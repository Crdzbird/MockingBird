// Backends without an OS capture timestamp stamp a chunk with now() minus
// the chunk's own duration, which approximates the first sample's time.
#pragma once

#include "clock.hpp"

#include <cstdint>

namespace mockingbird {

inline uint64_t chunk_start_ts_us(uint32_t sample_rate_hz, uint32_t frame_count) noexcept {
    const uint64_t now = monotonic_now_us();
    const uint64_t chunk_us = (sample_rate_hz > 0)
        ? (static_cast<uint64_t>(frame_count) * 1'000'000ULL / sample_rate_hz)
        : 0;
    return (now > chunk_us) ? (now - chunk_us) : 0;
}

} // namespace mockingbird
