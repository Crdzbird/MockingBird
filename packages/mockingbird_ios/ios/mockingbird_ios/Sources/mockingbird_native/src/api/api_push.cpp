// mockingbird_push_pcm: the host-PCM producer side of the capture ring.
#include "mockingbird/mockingbird.h"
#include "engine.hpp"
#include "engine_ingest.hpp"
#include "worker/worker_driver.hpp"

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_push_pcm(mockingbird_engine_t* e, const int16_t* samples,
                                        uint32_t frame_count, uint64_t capture_ts_us) {
    if (e == nullptr || samples == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    if (e->cfg.backend != MOCKINGBIRD_BACKEND_HOST_PCM) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (e->worker_fatal.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_INTERNAL;
    if (!e->running.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_NOT_RUNNING;
    if (frame_count == 0) return MOCKINGBIRD_OK;
    if (frame_count > e->pcm_ring.capacity()) return MOCKINGBIRD_ERR_INVALID_ARG;
    mockingbird::ingest_pcm(e, samples, frame_count, capture_ts_us);
    mockingbird::driver_pump(e);
    return MOCKINGBIRD_OK;
}

}
