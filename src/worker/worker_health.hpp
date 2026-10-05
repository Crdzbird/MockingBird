// Per-frame processing cost, a peak hold, and a ~1 s busy window that also
// drives adaptive degradation.
#pragma once

#include <chrono>
#include <cstdint>

struct mockingbird_engine;

namespace mockingbird {

class Tunables;

class WorkerHealth {
public:
    void start(const mockingbird_engine* e);
    void begin_frame();
    void end_frame(mockingbird_engine* e, uint64_t now_ts, Tunables& t);

private:
    using clock = std::chrono::steady_clock;

    void close_window(mockingbird_engine* e, uint64_t now_ts, Tunables& t, uint64_t window_ns);

    clock::time_point win_start_;
    clock::time_point frame_t0_;
    uint64_t          win_busy_ns_  = 0;
    uint64_t          drops_seen_   = 0;
    uint32_t          max_frame_us_ = 0;
};

} // namespace mockingbird
