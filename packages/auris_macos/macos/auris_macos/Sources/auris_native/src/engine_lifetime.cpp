// The engine's constructor and destructor live here because the inline
// worker is only forward-declared in the header.
#include "engine.hpp"
#include "worker/worker.hpp"

auris_engine::auris_engine(const auris_config_t& c, size_t ring_frames,
                           size_t pool_capacity, size_t max_payload_bytes)
    : cfg(c),
      pcm_ring(ring_frames),
      packets(pool_capacity, max_payload_bytes),
      live_cfg(c) {}

auris_engine::~auris_engine() = default;
