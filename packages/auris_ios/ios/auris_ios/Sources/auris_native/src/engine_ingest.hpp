// The single entry point for PCM arriving from any producer: the host's
// auris_push_pcm or the capture backend's real-time thread.
#pragma once

#include <cstdint>

struct auris_engine;

namespace auris {

void ingest_pcm(auris_engine*  e,
                const int16_t* samples,
                uint32_t       frame_count,
                uint64_t       capture_ts_us) noexcept;

void backend_pcm_callback(void*          user,
                          const int16_t* samples,
                          uint32_t       frame_count,
                          uint64_t       capture_ts_us) noexcept;

} // namespace auris
