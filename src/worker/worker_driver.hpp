// How the worker is driven: on its own thread, or inline on the producer's
// thread where the platform has no threads (WebAssembly without pthreads).
#pragma once

struct mockingbird_engine;

namespace mockingbird {
void driver_start(mockingbird_engine* e);
void driver_pump(mockingbird_engine* e) noexcept;
void driver_stop(mockingbird_engine* e);

}
