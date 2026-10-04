// auris_start and auris_stop. Capture starts last and stops first, so frames
// only flow while the worker is there to consume them.
#include "auris/auris.h"
#include "clock.hpp"
#include "engine.hpp"
#include "engine_ingest.hpp"
#include "worker/worker.hpp"

namespace {

void stop_worker(auris_engine* e) {
    e->stop_requested.store(true, std::memory_order_release);
    e->wake_count.fetch_add(1, std::memory_order_release);
    e->wake_count.notify_one();
    if (e->worker_thread.joinable()) e->worker_thread.join();
}

} // namespace

extern "C" {

AURIS_API auris_status_t auris_start(auris_engine_t* e) {
    if (e == nullptr) return AURIS_ERR_INVALID_ARG;
    std::lock_guard lk(e->lifecycle_mu);
    if (e->running.load(std::memory_order_acquire)) return AURIS_ERR_ALREADY_RUNNING;
    try {
        e->stop_requested.store(false, std::memory_order_release);
        e->last_capture_ts_us.store(auris::monotonic_now_us(), std::memory_order_release);
        e->first_capture_ts_us.store(~uint64_t{0}, std::memory_order_release);
        if (e->capture_backend) {
            const auris_status_t r =
                e->capture_backend->init(e->cfg, auris::backend_pcm_callback, e);
            if (r != AURIS_OK) return r;
        }
        e->worker_thread = std::thread(auris::worker_loop, e);
        if (e->capture_backend) {
            const auris_status_t r = e->capture_backend->start();
            if (r != AURIS_OK) {
                stop_worker(e);
                return r;
            }
        }
        e->running.store(true, std::memory_order_release);
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

AURIS_API auris_status_t auris_stop(auris_engine_t* e) {
    if (e == nullptr) return AURIS_ERR_INVALID_ARG;
    std::lock_guard lk(e->lifecycle_mu);
    if (!e->running.load(std::memory_order_acquire)) return AURIS_ERR_NOT_RUNNING;
    try {
        if (e->capture_backend) e->capture_backend->stop();
        stop_worker(e);
        e->running.store(false, std::memory_order_release);
        return AURIS_OK;
    } catch (...) {
        return AURIS_ERR_INTERNAL;
    }
}

} // extern "C"
