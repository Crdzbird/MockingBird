// The worker thread: drains the capture ring frame by frame through the
// level meter, denoiser, detectors, classifier and segmenter, then parks.
#pragma once

#include "frame_clock.hpp"
#include "worker/detector_bank.hpp"
#include "worker/segmenter.hpp"
#include "worker/speaker_verifier.hpp"
#include "worker/tunables.hpp"
#include "worker/worker_health.hpp"

#include <cstdint>
#include <vector>

struct mockingbird_engine;

namespace mockingbird {
class Worker {
public:
    explicit Worker(mockingbird_engine* e);

    void run();
    bool drain_ring();
    void finish();

private:
    void process_frame(uint64_t now_ts);
    void tap(const int16_t* samples);
    void park();

    mockingbird_engine*        e_;
    uint32_t             frame_samples_;
    std::vector<int16_t> scratch_;
    FrameClock           clock_;
    Tunables             tunables_;
    DetectorBank         bank_;
    Segmenter            segmenter_;
    SpeakerVerifier      speaker_;
    WorkerHealth         health_;
    uint64_t             last_seen_wake_ = 0;
};

void worker_loop(mockingbird_engine* e) noexcept;

}
