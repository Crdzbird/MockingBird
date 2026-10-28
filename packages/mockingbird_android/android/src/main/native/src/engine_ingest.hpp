// The single entry point for PCM arriving from any producer: the host's
// mockingbird_push_pcm or the capture backend's real-time thread.
#pragma once

#include <cstdint>

struct mockingbird_engine;

namespace mockingbird {
void ingest_pcm(mockingbird_engine*  e,
                const int16_t* samples,
                uint32_t       frame_count,
                uint64_t       capture_ts_us) noexcept;

void backend_pcm_callback(void*          user,
                          const int16_t* samples,
                          uint32_t       frame_count,
                          uint64_t       capture_ts_us) noexcept;

}
