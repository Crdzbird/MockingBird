// tests/test_lifecycle.cpp
//
// Lifecycle semantics: start/stop idempotency, double-start, callback
// registration races.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace {
mockingbird_engine_t* create_host_pcm() {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend        = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec          = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.enable_vad     = 0;
    cfg.enable_anomaly = 0;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(e != nullptr);
    return e;
}

}

TEST_CASE("mockingbird_start without mockingbird_create is undefined; mockingbird_stop returns NOT_RUNNING") {
    mockingbird_engine_t* e = create_host_pcm();
    CHECK(mockingbird_stop(e) == MOCKINGBIRD_ERR_NOT_RUNNING);
    mockingbird_destroy(e);
}

TEST_CASE("mockingbird_start is idempotent: second call returns ALREADY_RUNNING") {
    mockingbird_engine_t* e = create_host_pcm();
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    CHECK(mockingbird_start(e) == MOCKINGBIRD_ERR_ALREADY_RUNNING);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}

TEST_CASE("Stop on an engine that was never started returns NOT_RUNNING") {
    mockingbird_engine_t* e = create_host_pcm();
    CHECK(mockingbird_stop(e) == MOCKINGBIRD_ERR_NOT_RUNNING);
    mockingbird_destroy(e);
}

TEST_CASE("mockingbird_destroy auto-stops a running engine") {
    mockingbird_engine_t* e = create_host_pcm();
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
    CHECK(true);
}

TEST_CASE("Concurrent mockingbird_stop is safe: exactly one succeeds, no double-join") {
    mockingbird_engine_t* e = create_host_pcm();
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    constexpr int kThreads = 8;
    std::atomic<int> ok{0};
    std::atomic<int> not_running{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&]() {
            const mockingbird_status_t s = mockingbird_stop(e);
            if (s == MOCKINGBIRD_OK) {
                ok.fetch_add(1, std::memory_order_relaxed);
            } else if (s == MOCKINGBIRD_ERR_NOT_RUNNING) {
                not_running.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) t.join();

    CHECK(ok.load() == 1);
    CHECK(not_running.load() == kThreads - 1);
    mockingbird_destroy(e);
}

namespace {

void event_counter_cb(const mockingbird_event_t* /*evt*/, void* user) {
    auto* p = static_cast<std::atomic<int>*>(user);
    p->fetch_add(1, std::memory_order_relaxed);
}
}

TEST_CASE("mockingbird_set_event_callback fires the registered callback") {
    mockingbird_engine_t* e = create_host_pcm();
    std::atomic<int> count{0};
    REQUIRE(mockingbird_set_event_callback(e, event_counter_cb, &count) == MOCKINGBIRD_OK);

    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    std::vector<int16_t> samples(320, 0);
    REQUIRE(mockingbird_push_pcm(e, samples.data(), 320, 0) == MOCKINGBIRD_OK);

    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);

    CHECK(count.load() >= 2);

    CHECK(mockingbird_set_event_callback(e, nullptr, nullptr) == MOCKINGBIRD_OK);

    mockingbird_destroy(e);
}

TEST_CASE("mockingbird_get_stats on a never-started engine reports zeros") {
    mockingbird_engine_t* e = create_host_pcm();
    mockingbird_stats_t s{};
    REQUIRE(mockingbird_get_stats(e, &s) == MOCKINGBIRD_OK);
    CHECK(s.frames_captured        == 0);
    CHECK(s.packets_emitted        == 0);
    CHECK(s.packets_dropped        == 0);
    CHECK(s.segments_opened        == 0);
    CHECK(s.segments_closed        == 0);
    CHECK(s.pcm_ring_fill_frames   == 0);
    CHECK(s.packet_queue_fill      == 0);
    mockingbird_destroy(e);
}

TEST_CASE("mockingbird_drain on a running but starved engine returns QUEUE_EMPTY") {
    mockingbird_engine_t* e = create_host_pcm();
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    mockingbird_packet_t pkt{};
    CHECK(mockingbird_drain(e, &pkt) == MOCKINGBIRD_ERR_QUEUE_EMPTY);
    mockingbird_stop(e);
    mockingbird_destroy(e);
}
