// The lifecycle thread behind mockingbird_submit and mockingbird_player_submit.
#include "lifecycle_thread.hpp"

#include <new>
#include <utility>

namespace mockingbird {
LifecycleThread::~LifecycleThread() { close(); }

mockingbird_status_t LifecycleThread::post(std::function<void()> job) {
    try {
        std::lock_guard lk(mu_);
        if (closed_) return MOCKINGBIRD_ERR_INVALID_STATE;
        jobs_.push_back(std::move(job));
        if (!thread_.joinable()) {
            try {
                thread_ = std::thread([this] { run(); });
            } catch (...) {
                jobs_.pop_back();
                throw;
            }
        }
    } catch (const std::bad_alloc&) {
        return MOCKINGBIRD_ERR_NO_MEMORY;
    } catch (...) {
        return MOCKINGBIRD_ERR_INTERNAL;
    }
    cv_.notify_one();
    return MOCKINGBIRD_OK;
}

void LifecycleThread::close() {
    {
        std::lock_guard lk(mu_);
        closed_ = true;
    }
    cv_.notify_one();
    if (thread_.joinable()) thread_.join();
}

void LifecycleThread::run() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock lk(mu_);
            cv_.wait(lk, [this] { return closed_ || !jobs_.empty(); });
            if (jobs_.empty()) return;
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        job();
    }
}

}
