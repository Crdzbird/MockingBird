// The worker thread: drains the capture ring frame by frame through the
// level meter, denoiser, detectors, classifier and segmenter, then parks.
#pragma once

#include "frame_clock.hpp"
#include "worker/detector_bank.hpp"
#include "worker/segmenter.hpp"
#include "worker/tunables.hpp"
#include "worker/worker_health.hpp"

#include <cstdint>
#include <vector>

struct auris_engine;

namespace auris {

class Worker {
public:
    explicit Worker(auris_engine* e);

    void run();

private:
    bool drain_ring();
    void process_frame(uint64_t now_ts);
    void tap(const int16_t* samples);
    void park();

    auris_engine*        e_;
    uint32_t             frame_samples_;
    std::vector<int16_t> scratch_;
    FrameClock           clock_;
    Tunables             tunables_;
    DetectorBank         bank_;
    Segmenter            segmenter_;
    WorkerHealth         health_;
    uint64_t             last_seen_wake_ = 0;
};

void worker_loop(auris_engine* e) noexcept;

} // namespace auris
