// Inline driver: each mockingbird_push_pcm drains the ring on the caller's thread,
// so the engine runs without any thread of its own.
#ifdef __EMSCRIPTEN__

#include "worker/worker_driver.hpp"

#include "engine.hpp"
#include "engine_events.hpp"
#include "worker/worker.hpp"

#include <memory>

namespace mockingbird {
void driver_start(mockingbird_engine* e) {
    e->inline_worker = std::make_unique<Worker>(e);
}

void driver_pump(mockingbird_engine* e) noexcept {
    if (!e->inline_worker) return;
    try {
        e->inline_worker->drain_ring();
    } catch (...) {
        e->worker_fatal.store(true, std::memory_order_release);
        emit_backend_error(e, MOCKINGBIRD_ERR_INTERNAL);
    }
}

void driver_stop(mockingbird_engine* e) {
    if (!e->inline_worker) return;
    try {
        e->inline_worker->drain_ring();
        e->inline_worker->finish();
    } catch (...) {
        e->worker_fatal.store(true, std::memory_order_release);
    }
    e->inline_worker.reset();
}

}

#endif
