// A thread that runs lifecycle jobs one at a time in the order posted, so
// opening and closing a device happens off the caller's thread. It starts on
// the first post, which returns MOCKINGBIRD_OK once the job is queued. close
// refuses further posts, runs what is still queued and joins; it is
// idempotent. A job must not throw and must not close its own thread.
#pragma once

#include "mockingbird/mockingbird.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace mockingbird {
class LifecycleThread {
public:
    LifecycleThread() = default;
    LifecycleThread(const LifecycleThread&)            = delete;
    LifecycleThread& operator=(const LifecycleThread&) = delete;
    ~LifecycleThread();

    mockingbird_status_t post(std::function<void()> job);
    void close();

private:
    void run();

    std::mutex                        mu_;
    std::condition_variable           cv_;
    std::deque<std::function<void()>> jobs_;
    std::thread                       thread_;
    bool                              closed_ = false;
};

}
