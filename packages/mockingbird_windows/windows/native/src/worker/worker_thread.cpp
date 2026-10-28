// Threaded driver: the worker runs on its own std::thread and parks on the
// wake counter between batches.
#ifndef __EMSCRIPTEN__

#include "worker/worker_driver.hpp"

#include "engine.hpp"
#include "worker/worker.hpp"

namespace mockingbird {
void driver_start(mockingbird_engine* e) {
    e->worker_thread = std::thread(worker_loop, e);
}

void driver_pump(mockingbird_engine*) noexcept {}

void driver_stop(mockingbird_engine* e) {
    e->stop_requested.store(true, std::memory_order_release);
    e->wake_count.fetch_add(1, std::memory_order_release);
    e->wake_count.notify_one();
    if (e->worker_thread.joinable()) e->worker_thread.join();
}

}

#endif
