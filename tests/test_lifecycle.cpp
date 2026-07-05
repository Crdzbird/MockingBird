// tests/test_lifecycle.cpp
//
// Lifecycle semantics: start/stop idempotency, double-start, callback
// registration races.

#include "doctest.h"
#include "auris/auris.h"

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

auris_engine_t* create_host_pcm() {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend        = AURIS_BACKEND_HOST_PCM;
    cfg.codec          = AURIS_CODEC_PCM_S16LE;
    cfg.enable_vad     = 0;
    cfg.enable_anomaly = 0;
    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);
    REQUIRE(e != nullptr);
    return e;
}

} // namespace

TEST_CASE("auris_start without auris_create is undefined; auris_stop returns NOT_RUNNING") {
    auris_engine_t* e = create_host_pcm();
    CHECK(auris_stop(e) == AURIS_ERR_NOT_RUNNING);
    auris_destroy(e);
}

TEST_CASE("auris_start is idempotent: second call returns ALREADY_RUNNING") {
    auris_engine_t* e = create_host_pcm();
    REQUIRE(auris_start(e) == AURIS_OK);
    CHECK(auris_start(e) == AURIS_ERR_ALREADY_RUNNING);
    REQUIRE(auris_stop(e) == AURIS_OK);
    auris_destroy(e);
}

TEST_CASE("Stop on an engine that was never started returns NOT_RUNNING") {
    auris_engine_t* e = create_host_pcm();
    CHECK(auris_stop(e) == AURIS_ERR_NOT_RUNNING);
    auris_destroy(e);
}

TEST_CASE("auris_destroy auto-stops a running engine") {
    auris_engine_t* e = create_host_pcm();
    REQUIRE(auris_start(e) == AURIS_OK);
    // No explicit stop: destroy must join cleanly.
    auris_destroy(e);
    CHECK(true);                    // surviving here is the assertion
}

TEST_CASE("Concurrent auris_stop is safe: exactly one succeeds, no double-join") {
    // Regression for the lifecycle mutex: without it, two threads could both
    // pass the joinable() check and join the same worker thread -> UB /
    // std::terminate. Run under TSan to also prove there's no data race.
    auris_engine_t* e = create_host_pcm();
    REQUIRE(auris_start(e) == AURIS_OK);

    constexpr int kThreads = 8;
    std::atomic<int> ok{0};
    std::atomic<int> not_running{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&]() {
            const auris_status_t s = auris_stop(e);
            if (s == AURIS_OK) {
                ok.fetch_add(1, std::memory_order_relaxed);
            } else if (s == AURIS_ERR_NOT_RUNNING) {
                not_running.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) t.join();

    CHECK(ok.load() == 1);
    CHECK(not_running.load() == kThreads - 1);
    auris_destroy(e);
}

namespace {
std::atomic<int> g_event_count{0};

void event_counter_cb(const auris_event_t* /*evt*/, void* user) {
    auto* p = static_cast<std::atomic<int>*>(user);
    p->fetch_add(1, std::memory_order_relaxed);
}
} // namespace

TEST_CASE("auris_set_event_callback fires the registered callback") {
    auris_engine_t* e = create_host_pcm();
    std::atomic<int> count{0};
    REQUIRE(auris_set_event_callback(e, event_counter_cb, &count) == AURIS_OK);

    REQUIRE(auris_start(e) == AURIS_OK);

    // Push a frame so the worker opens / closes a segment, generating
    // BEGIN + END events.
    std::vector<int16_t> samples(320, 0);
    REQUIRE(auris_push_pcm(e, samples.data(), 320, 0) == AURIS_OK);

    REQUIRE(auris_stop(e) == AURIS_OK);

    CHECK(count.load() >= 2);       // at least SEGMENT_BEGIN + SEGMENT_END

    // Unregister + verify no further calls (no easy way to verify
    // "definitely no more calls" without races; check that we can clear it
    // cleanly).
    CHECK(auris_set_event_callback(e, nullptr, nullptr) == AURIS_OK);

    auris_destroy(e);
}

TEST_CASE("auris_get_stats on a never-started engine reports zeros") {
    auris_engine_t* e = create_host_pcm();
    auris_stats_t s{};
    REQUIRE(auris_get_stats(e, &s) == AURIS_OK);
    CHECK(s.frames_captured        == 0);
    CHECK(s.packets_emitted        == 0);
    CHECK(s.packets_dropped        == 0);
    CHECK(s.segments_opened        == 0);
    CHECK(s.segments_closed        == 0);
    CHECK(s.pcm_ring_fill_frames   == 0);
    CHECK(s.packet_queue_fill      == 0);
    auris_destroy(e);
}

TEST_CASE("auris_drain on a running but starved engine returns QUEUE_EMPTY") {
    auris_engine_t* e = create_host_pcm();
    REQUIRE(auris_start(e) == AURIS_OK);
    auris_packet_t pkt{};
    CHECK(auris_drain(e, &pkt) == AURIS_ERR_QUEUE_EMPTY);
    auris_stop(e);
    auris_destroy(e);
}
