// auris_push_pcm: the host-PCM producer side of the capture ring.
#include "auris/auris.h"
#include "engine.hpp"
#include "engine_ingest.hpp"
#include "worker/worker_driver.hpp"

extern "C" {

AURIS_API auris_status_t auris_push_pcm(auris_engine_t* e, const int16_t* samples,
                                        uint32_t frame_count, uint64_t capture_ts_us) {
    if (e == nullptr || samples == nullptr) return AURIS_ERR_INVALID_ARG;
    if (e->cfg.backend != AURIS_BACKEND_HOST_PCM) return AURIS_ERR_INVALID_STATE;
    if (e->worker_fatal.load(std::memory_order_acquire)) return AURIS_ERR_INTERNAL;
    if (!e->running.load(std::memory_order_acquire)) return AURIS_ERR_NOT_RUNNING;
    if (frame_count == 0) return AURIS_OK;
    if (frame_count > e->pcm_ring.capacity()) return AURIS_ERR_INVALID_ARG;
    auris::ingest_pcm(e, samples, frame_count, capture_ts_us);
    auris::driver_pump(e);
    return AURIS_OK;
}

} // extern "C"
