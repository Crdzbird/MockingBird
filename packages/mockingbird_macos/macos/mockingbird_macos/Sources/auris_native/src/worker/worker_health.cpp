// The window closes from inside the frame loop, so it only advances while
// audio flows; an idle engine keeps reporting its last busy share.
#include "worker/worker_health.hpp"

#include "auris_log.hpp"
#include "engine.hpp"
#include "engine_events.hpp"
#include "worker/tunables.hpp"

namespace auris {

namespace {

constexpr uint64_t kHealthWindowNs = 1000000000ull;

uint64_t ns_between(std::chrono::steady_clock::time_point a,
                    std::chrono::steady_clock::time_point b) {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count());
}

} // namespace

void WorkerHealth::start(const auris_engine* e) {
    win_start_  = clock::now();
    drops_seen_ = e->frames_dropped_capture.load(std::memory_order_relaxed);
}

void WorkerHealth::begin_frame() { frame_t0_ = clock::now(); }

void WorkerHealth::end_frame(auris_engine* e, uint64_t now_ts, Tunables& t) {
    const auto     t1 = clock::now();
    const uint64_t ns = ns_between(frame_t0_, t1);
    win_busy_ns_ += ns;
    const uint32_t us = static_cast<uint32_t>(ns / 1000u);
    if (us > max_frame_us_) {
        max_frame_us_ = us;
        e->worker_max_frame_us.store(us, std::memory_order_relaxed);
    }
    const uint64_t window_ns = ns_between(win_start_, t1);
    if (window_ns < kHealthWindowNs) return;
    close_window(e, now_ts, t, window_ns);
    win_start_   = t1;
    win_busy_ns_ = 0;
}

void WorkerHealth::close_window(auris_engine* e, uint64_t now_ts, Tunables& t,
                                uint64_t window_ns) {
    uint64_t ppm = (win_busy_ns_ * 1000000ull) / window_ns;
    if (ppm > 1000000ull) ppm = 1000000ull;
    e->worker_busy_ppm.store(static_cast<uint32_t>(ppm), std::memory_order_relaxed);

    const uint64_t drops_now = e->frames_dropped_capture.load(std::memory_order_relaxed);
    const bool     dropped   = drops_now != drops_seen_;
    drops_seen_              = drops_now;

    if (!t.adaptive_on || !t.degrade.update(static_cast<uint32_t>(ppm), dropped)) return;
    e->degradation_level.store(t.degrade.level(), std::memory_order_relaxed);
    emit_event(e, make_event(AURIS_EVT_DEGRADATION, 0, now_ts, t.degrade.level(),
                             static_cast<float>(ppm) / 1000000.0f));
    AURIS_VLOG("degradation level -> %u (busy %llu ppm%s)", t.degrade.level(),
               static_cast<unsigned long long>(ppm), dropped ? ", drops" : "");
}

} // namespace auris
