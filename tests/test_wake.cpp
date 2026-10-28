// tests/test_wake.cpp
//
// The wake callback: one call per burst until the host acknowledges it, a new
// call for anything queued after the acknowledgement, and none once it is
// unregistered.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {
void count_wake(void* user) {
    static_cast<std::atomic<int>*>(user)->fetch_add(1, std::memory_order_relaxed);
}

mockingbird_engine_t* passthrough_engine() {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend        = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.enable_vad     = 0;
    cfg.enable_anomaly = 0;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    return e;
}

void push_frames(mockingbird_engine_t* e, uint32_t n, uint64_t& ts) {
    const std::vector<int16_t> frame(320, 100);
    for (uint32_t i = 0; i < n; ++i) {
        REQUIRE(mockingbird_push_pcm(e, frame.data(), 320, ts) == MOCKINGBIRD_OK);
        ts += 20'000ull;
    }
}

bool wait_for(const std::atomic<int>& v, int at_least) {
    for (int i = 0; i < 200 && v.load() < at_least; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return v.load() >= at_least;
}

void drain_all(mockingbird_engine_t* e) {
    mockingbird_packet_t pkt{};
    while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) mockingbird_release(e, &pkt);
    mockingbird_event_t evt{};
    while (mockingbird_poll_event(e, &evt) == MOCKINGBIRD_OK) {
    }
}

}

TEST_CASE("wake fires once per burst until acknowledged, then again") {
    mockingbird_engine_t* e = passthrough_engine();
    std::atomic<int> wakes{0};
    REQUIRE(mockingbird_set_wake_callback(e, count_wake, &wakes) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    uint64_t ts = 0;
    push_frames(e, 20, ts);
    REQUIRE(wait_for(wakes, 1));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(wakes.load() == 1);

    REQUIRE(mockingbird_ack_wake(e) == MOCKINGBIRD_OK);
    drain_all(e);
    push_frames(e, 5, ts);
    REQUIRE(wait_for(wakes, 2));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(wakes.load() == 2);

    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}

TEST_CASE("an unregistered wake never fires, and registering re-arms it") {
    mockingbird_engine_t* e = passthrough_engine();
    std::atomic<int> wakes{0};
    REQUIRE(mockingbird_set_wake_callback(e, count_wake, &wakes) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_set_wake_callback(e, nullptr, nullptr) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    uint64_t ts = 0;
    push_frames(e, 10, ts);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    CHECK(wakes.load() == 0);

    REQUIRE(mockingbird_set_wake_callback(e, count_wake, &wakes) == MOCKINGBIRD_OK);
    push_frames(e, 3, ts);
    CHECK(wait_for(wakes, 1));

    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}

TEST_CASE("wake rejects a null engine") {
    CHECK(mockingbird_set_wake_callback(nullptr, nullptr, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_ack_wake(nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
}
