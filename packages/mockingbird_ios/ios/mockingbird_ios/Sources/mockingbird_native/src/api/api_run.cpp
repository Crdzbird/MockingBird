// mockingbird_start and mockingbird_stop. Capture starts last and stops first, so frames
// only flow while the worker is there to consume them. Each start after the
// first opens a fresh capture backend, so an engine can be started again and
// picks up a device or route that changed while it was stopped.
#include "mockingbird/mockingbird.h"
#include "clock.hpp"
#include "engine.hpp"
#include "engine_ingest.hpp"
#include "worker/worker_driver.hpp"

extern "C" {
MOCKINGBIRD_API mockingbird_status_t mockingbird_start(mockingbird_engine_t* e) {
    if (e == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    std::lock_guard lk(e->lifecycle_mu);
    if (e->running.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
    try {
        e->stop_requested.store(false, std::memory_order_release);
        e->last_capture_ts_us.store(mockingbird::monotonic_now_us(), std::memory_order_release);
        e->first_capture_ts_us.store(~uint64_t{0}, std::memory_order_release);
        if (e->capture_backend && e->capture_backend_used) {
            e->capture_backend = mockingbird::make_backend(e->cfg.backend);
            if (!e->capture_backend) return MOCKINGBIRD_ERR_UNSUPPORTED;
        }
        if (e->capture_backend) {
            e->capture_backend_used = true;
            e->capture_backend->set_notice_callback(mockingbird::backend_notice_callback, e);
            mockingbird_status_t r = e->capture_backend->select_device(e->input_device_id);
            if (r == MOCKINGBIRD_OK) {
                r = e->capture_backend->init(e->cfg, mockingbird::backend_pcm_callback, e);
            }
            if (r != MOCKINGBIRD_OK) return r;
        }
        mockingbird::driver_start(e);
        if (e->capture_backend) {
            const mockingbird_status_t r = e->capture_backend->start();
            if (r != MOCKINGBIRD_OK) {
                mockingbird::driver_stop(e);
                return r;
            }
        }
        if (e->capture_backend) {
            e->voice_processing_active.store(e->capture_backend->voice_processing_active(),
                                             std::memory_order_relaxed);
        }
        e->running.store(true, std::memory_order_release);
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

MOCKINGBIRD_API mockingbird_status_t mockingbird_stop(mockingbird_engine_t* e) {
    if (e == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    std::lock_guard lk(e->lifecycle_mu);
    if (!e->running.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_NOT_RUNNING;
    try {
        if (e->capture_backend) e->capture_backend->stop();
        mockingbird::driver_stop(e);
        e->running.store(false, std::memory_order_release);
        return MOCKINGBIRD_OK;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
}

}
