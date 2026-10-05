// Stop is honoured only after the ring is drained, so every frame pushed
// before mockingbird_stop is processed. No exception may leave this thread.
#include "worker/worker.hpp"

#include "config_validation.hpp"
#include "engine.hpp"
#include "engine_events.hpp"

namespace mockingbird {

Worker::Worker(mockingbird_engine* e)
    : e_(e),
      frame_samples_(samples_per_frame(e->cfg)),
      scratch_(frame_samples_),
      clock_(e->first_capture_ts_us, e->cfg.frame_ms) {
    bank_.init(e);
    tunables_.load(e);
    segmenter_.init(e, frame_samples_);
    health_.start(e);
}

void Worker::run() {
    while (true) {
        if (drain_ring()) {
            last_seen_wake_ = e_->wake_count.load(std::memory_order_acquire);
            continue;
        }
        if (e_->stop_requested.load(std::memory_order_acquire)) break;
        park();
    }
    finish();
}

void Worker::finish() {
    segmenter_.finish(clock_.now());
}

bool Worker::drain_ring() {
    bool did_work = false;
    while (e_->pcm_ring.size_relaxed() >= frame_samples_) {
        if (e_->pcm_ring.pop(scratch_.data(), frame_samples_) < frame_samples_) break;
        did_work = true;
        process_frame(clock_.next());
    }
    return did_work;
}

void Worker::park() {
    const uint64_t cur = e_->wake_count.load(std::memory_order_acquire);
    if (cur == last_seen_wake_) {
        e_->wake_count.wait(cur, std::memory_order_acquire);
        e_->worker_wakeups.fetch_add(1, std::memory_order_relaxed);
    }
    last_seen_wake_ = e_->wake_count.load(std::memory_order_acquire);
}

void Worker::tap(const int16_t* samples) {
    if (!e_->pcm_tap) return;
    const size_t wrote = e_->pcm_tap->push(samples, frame_samples_);
    if (wrote < frame_samples_) {
        e_->tap_samples_dropped.fetch_add(frame_samples_ - wrote, std::memory_order_relaxed);
    }
}

void worker_loop(mockingbird_engine* e) noexcept {
    try {
        Worker worker(e);
        worker.run();
    } catch (...) {
        e->worker_fatal.store(true, std::memory_order_release);
        try {
            emit_backend_error(e, MOCKINGBIRD_ERR_INTERNAL);
        } catch (...) {
        }
    }
}

} // namespace mockingbird
