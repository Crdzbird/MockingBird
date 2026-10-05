// How the worker is driven: on its own thread, or inline on the producer's
// thread where the platform has no threads (WebAssembly without pthreads).
#pragma once

struct auris_engine;

namespace auris {

void driver_start(auris_engine* e);
void driver_pump(auris_engine* e) noexcept;
void driver_stop(auris_engine* e);

} // namespace auris
