// tests/test_wake_release.cpp
//
// The host may free its wake the moment it unregisters it. A busy worker
// keeps queueing packets while the host unregisters and frees, retunes,
// stops, closes and destroys, and the wake must never run after
// mockingbird_set_wake_callback(NULL) or mockingbird_set_wake_port(NULL)
// returned. Each registration owns a heap box that is deleted right after
// unregistering, so a late call is a use-after-free AddressSanitizer reports;
// ThreadSanitizer checks the handoff. The port variant also covers a port
// the host closed without unregistering, as a Dart isolate does when it
// shuts down, and the done message of mockingbird_submit_port.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "host_post.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace {
struct WakeBox {
    std::atomic<int>  calls{0};
    std::atomic<bool> released{false};
};

std::atomic<int> g_late_wakes{0};

void box_wake(void* user) {
    auto* box = static_cast<WakeBox*>(user);
    if (box->released.load(std::memory_order_acquire)) g_late_wakes.fetch_add(1);
    box->calls.fetch_add(1, std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::microseconds(50));
}

struct PortLog {
    std::mutex                    mu;
    std::condition_variable       cv;
    std::set<int64_t>             closed;
    std::set<int64_t>             unregistered;
    std::vector<int64_t>          done_values;
    std::atomic<int>              posts{0};
    std::atomic<int>              posts_to_closed{0};
    std::atomic<int>              posts_after_unregister{0};
};

PortLog* g_ports = nullptr;

int8_t fake_post(int64_t port, void* message) {
    auto* m = static_cast<mockingbird::HostMessage*>(message);
    std::lock_guard lk(g_ports->mu);
    g_ports->posts.fetch_add(1);
    if (g_ports->unregistered.count(port) != 0) g_ports->posts_after_unregister.fetch_add(1);
    if (g_ports->closed.count(port) != 0) {
        g_ports->posts_to_closed.fetch_add(1);
        return 0;
    }
    if (m->type == mockingbird::kHostMessageInt64) {
        g_ports->done_values.push_back(m->value.as_int64);
        g_ports->cv.notify_all();
    }
    return 1;
}

struct PortScope {
    PortLog log;
    PortScope() { g_ports = &log; }
    ~PortScope() { g_ports = nullptr; }
};

mockingbird_engine_t* busy_engine() {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend         = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.enable_vad      = 0;
    cfg.enable_anomaly  = 0;
    cfg.pcm_ring_frames = 1u << 18;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    return e;
}

void push_backlog(mockingbird_engine_t* e, uint32_t frames, uint64_t& ts) {
    const std::vector<int16_t> frame(320, 200);
    for (uint32_t i = 0; i < frames; ++i) {
        mockingbird_push_pcm(e, frame.data(), 320, ts);
        ts += 20'000ull;
    }
}

void drain_all(mockingbird_engine_t* e) {
    mockingbird_packet_t pkt{};
    while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) mockingbird_release(e, &pkt);
    mockingbird_event_t evt{};
    while (mockingbird_poll_event(e, &evt) == MOCKINGBIRD_OK) {
    }
}

void spin_host(mockingbird_engine_t* e, std::chrono::microseconds span) {
    const auto until = std::chrono::steady_clock::now() + span;
    while (std::chrono::steady_clock::now() < until) {
        mockingbird_ack_wake(e);
        drain_all(e);
        std::this_thread::yield();
    }
}

}

TEST_CASE("a freed wake callback is never called while the worker stays busy") {
    g_late_wakes.store(0);
    mockingbird_engine_t* e = busy_engine();
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    mockingbird_config_t tuned{};
    mockingbird_config_defaults(&tuned);
    tuned.backend         = MOCKINGBIRD_BACKEND_HOST_PCM;
    tuned.enable_vad      = 0;
    tuned.enable_anomaly  = 0;
    tuned.pcm_ring_frames = 1u << 18;
    uint64_t ts    = 0;
    int      calls = 0;
    for (int round = 0; round < 150; ++round) {
        auto* box = new WakeBox{};
        REQUIRE(mockingbird_set_wake_callback(e, box_wake, box) == MOCKINGBIRD_OK);
        push_backlog(e, 40, ts);
        spin_host(e, std::chrono::microseconds(300 + (round % 7) * 100));
        if (round % 10 == 0) {
            tuned.silence_gate_dbfs = round % 20 == 0 ? -50.0f : -60.0f;
            REQUIRE(mockingbird_update_config(e, &tuned) == MOCKINGBIRD_OK);
        }
        REQUIRE(mockingbird_set_wake_callback(e, nullptr, nullptr) == MOCKINGBIRD_OK);
        box->released.store(true, std::memory_order_release);
        calls += box->calls.load();
        delete box;
        push_backlog(e, 10, ts);
    }
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
    CHECK(calls > 0);
    CHECK(g_late_wakes.load() == 0);
}

TEST_CASE("stop, close and destroy race a busy worker after the wake is freed") {
    g_late_wakes.store(0);
    for (int round = 0; round < 40; ++round) {
        mockingbird_engine_t* e = busy_engine();
        auto* box = new WakeBox{};
        REQUIRE(mockingbird_set_wake_callback(e, box_wake, box) == MOCKINGBIRD_OK);
        REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
        uint64_t ts = 0;
        push_backlog(e, 600, ts);
        std::thread host([e, box, round] {
            std::this_thread::sleep_for(std::chrono::microseconds((round % 5) * 150));
            CHECK(mockingbird_set_wake_callback(e, nullptr, nullptr) == MOCKINGBIRD_OK);
            box->released.store(true, std::memory_order_release);
            delete box;
            if (round % 2 == 0) {
                CHECK(mockingbird_stop(e) == MOCKINGBIRD_OK);
            } else {
                CHECK(mockingbird_submit(e, MOCKINGBIRD_ENGINE_OP_CLOSE, nullptr, nullptr) ==
                        MOCKINGBIRD_OK);
            }
            mockingbird_destroy(e);
        });
        host.join();
    }
    CHECK(g_late_wakes.load() == 0);
}

TEST_CASE("destroy alone quiesces the worker before the wake is freed") {
    g_late_wakes.store(0);
    for (int round = 0; round < 20; ++round) {
        mockingbird_engine_t* e = busy_engine();
        auto* box = new WakeBox{};
        REQUIRE(mockingbird_set_wake_callback(e, box_wake, box) == MOCKINGBIRD_OK);
        REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
        uint64_t ts = 0;
        push_backlog(e, 400, ts);
        mockingbird_destroy(e);
        box->released.store(true, std::memory_order_release);
        delete box;
    }
    CHECK(g_late_wakes.load() == 0);
}

TEST_CASE("an unregistered wake port is never posted while the worker stays busy") {
    PortScope scope;
    mockingbird_engine_t* e = busy_engine();
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    uint64_t ts = 0;
    for (int64_t port = 1; port <= 150; ++port) {
        REQUIRE(mockingbird_set_wake_port(e, fake_post, port) == MOCKINGBIRD_OK);
        push_backlog(e, 40, ts);
        spin_host(e, std::chrono::microseconds(200 + (port % 7) * 100));
        if (port % 3 == 0) {
            auto* box = new WakeBox{};
            REQUIRE(mockingbird_set_wake_callback(e, box_wake, box) == MOCKINGBIRD_OK);
            {
                std::lock_guard lk(scope.log.mu);
                scope.log.unregistered.insert(port);
            }
            spin_host(e, std::chrono::microseconds(200));
            REQUIRE(mockingbird_set_wake_callback(e, nullptr, nullptr) == MOCKINGBIRD_OK);
            box->released.store(true, std::memory_order_release);
            delete box;
            continue;
        }
        REQUIRE(mockingbird_set_wake_port(e, nullptr, 0) == MOCKINGBIRD_OK);
        std::lock_guard lk(scope.log.mu);
        scope.log.unregistered.insert(port);
    }
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
    CHECK(scope.log.posts.load() > 0);
    CHECK(scope.log.posts_after_unregister.load() == 0);
}

TEST_CASE("a port closed without unregistering is harmless, as when a Dart isolate dies") {
    PortScope scope;
    mockingbird_engine_t* e = busy_engine();
    REQUIRE(mockingbird_set_wake_port(e, fake_post, 7) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    {
        std::lock_guard lk(scope.log.mu);
        scope.log.closed.insert(7);
    }
    uint64_t ts = 0;
    for (int round = 0; round < 30; ++round) {
        push_backlog(e, 20, ts);
        spin_host(e, std::chrono::microseconds(500));
    }
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
    CHECK(scope.log.posts_to_closed.load() > 0);
}

TEST_CASE("submit_port posts each op and its status as one int64 message") {
    PortScope scope;
    mockingbird_engine_t* e = busy_engine();
    REQUIRE(mockingbird_submit_port(e, MOCKINGBIRD_ENGINE_OP_START, fake_post, 3) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_submit_port(e, MOCKINGBIRD_ENGINE_OP_START, fake_post, 3) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_submit_port(e, MOCKINGBIRD_ENGINE_OP_CLOSE, fake_post, 3) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_submit_port(e, MOCKINGBIRD_ENGINE_OP_STOP, nullptr, 3) == MOCKINGBIRD_OK);
    {
        std::unique_lock lk(scope.log.mu);
        REQUIRE(scope.log.cv.wait_for(lk, std::chrono::seconds(5),
                                      [&] { return scope.log.done_values.size() >= 3; }));
    }
    mockingbird_destroy(e);
    const auto encode = [](uint32_t op, int32_t status) {
        return static_cast<int64_t>((static_cast<uint64_t>(op) << 32) | static_cast<uint32_t>(status));
    };
    REQUIRE(scope.log.done_values.size() == 3u);
    CHECK(scope.log.done_values[0] == encode(MOCKINGBIRD_ENGINE_OP_START, MOCKINGBIRD_OK));
    CHECK(scope.log.done_values[1] ==
          encode(MOCKINGBIRD_ENGINE_OP_START, MOCKINGBIRD_ERR_ALREADY_RUNNING));
    CHECK(scope.log.done_values[2] == encode(MOCKINGBIRD_ENGINE_OP_CLOSE, MOCKINGBIRD_OK));
    CHECK(mockingbird_submit_port(nullptr, MOCKINGBIRD_ENGINE_OP_START, fake_post, 3) ==
          MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_submit_port(nullptr, MOCKINGBIRD_PLAYER_OP_START, fake_post, 3) ==
          MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_set_wake_port(nullptr, fake_post, 3) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("the posted message has the layout of a Dart_CObject") {
    static_assert(sizeof(mockingbird::HostMessage) >= 8 + 5 * sizeof(void*));
    if constexpr (sizeof(void*) == 8) {
        static_assert(offsetof(mockingbird::HostMessage, value) == 8);
        static_assert(sizeof(mockingbird::HostMessage) == 48);
    }
    CHECK(mockingbird::kHostMessageNull == 0);
    CHECK(mockingbird::kHostMessageInt64 == 3);
}
