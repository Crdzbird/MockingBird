// Validation of a caller-supplied mockingbird_config_t before anything loads its
// enum fields, plus the derived sizes the engine computes from it.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace mockingbird {

constexpr uint16_t kChannelsV1 = 1;

template <typename E>
int enum_as_int(const E& field) {
    static_assert(sizeof(E) == sizeof(int), "ABI enum must be int-sized");
    int v;
    std::memcpy(&v, &field, sizeof(int));
    return v;
}

mockingbird_status_t validate_config(const mockingbird_config_t& c);

mockingbird_status_t check_detector_ranges(const mockingbird_config_t& c, int vad_tier, int codec);

mockingbird_status_t check_build_support(const mockingbird_config_t& c, int vad_tier, int codec);

uint32_t samples_per_frame(const mockingbird_config_t& c);

size_t batch_samples_for(const mockingbird_config_t& c, uint32_t batch_frames);

} // namespace mockingbird
