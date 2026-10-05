// Validation of a caller-supplied auris_config_t before anything loads its
// enum fields, plus the derived sizes the engine computes from it.
#pragma once

#include "auris/auris.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace auris {

constexpr uint16_t kChannelsV1 = 1;

template <typename E>
int enum_as_int(const E& field) {
    static_assert(sizeof(E) == sizeof(int), "ABI enum must be int-sized");
    int v;
    std::memcpy(&v, &field, sizeof(int));
    return v;
}

auris_status_t validate_config(const auris_config_t& c);

auris_status_t check_detector_ranges(const auris_config_t& c, int vad_tier, int codec);

auris_status_t check_build_support(const auris_config_t& c, int vad_tier, int codec);

uint32_t samples_per_frame(const auris_config_t& c);

size_t batch_samples_for(const auris_config_t& c, uint32_t batch_frames);

} // namespace auris
