// tests/test_wake_batching.cpp
//
// worker_batch_frames: the capture thread wakes the worker only once a batch
// is queued. Same frames, same output, far fewer wakeups.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <thread>
#include <vector>

namespace {
struct Engine { mockingbird_engine_t* e = nullptr; ~Engine() { if (e) { mockingbird_stop(e); mockingbird_destroy(e); } } };
constexpr uint32_t kFrame = 320;

mockingbird_config_t cfg_with_batch(uint32_t batch) {
    mockingbird_config_t c{}; mockingbird_config_defaults(&c);
    c.backend = MOCKINGBIRD_BACKEND_HOST_PCM; c.codec = MOCKINGBIRD_CODEC_PCM_S16LE;
    c.enable_vad = 0; c.enable_anomaly = 0;
    c.worker_batch_frames = batch;
    return c;
}

uint64_t wakeups_after(uint32_t batch, int frames) {
    Engine eng; mockingbird_config_t c = cfg_with_batch(batch);
    REQUIRE(mockingbird_create(&c, &eng.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(eng.e) == MOCKINGBIRD_OK);
    std::vector<int16_t> f(kFrame, 1000);
    for (int i = 0; i < frames; ++i) {
        REQUIRE(mockingbird_push_pcm(eng.e, f.data(), kFrame, static_cast<uint64_t>(i) * 20000) == MOCKINGBIRD_OK);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mockingbird_stats_t s{}; mockingbird_get_stats(eng.e, &s);
    mockingbird_stop(eng.e); mockingbird_get_stats(eng.e, &s);
    CHECK(s.frames_captured == static_cast<uint64_t>(frames) * kFrame);
    CHECK(s.packets_emitted == static_cast<uint64_t>(frames));
    return s.worker_wakeups;
}
}

TEST_CASE("batching: validation and defaults") {
    mockingbird_config_t c{}; mockingbird_config_defaults(&c);
    CHECK(c.worker_batch_frames == 0u);
    Engine bad; c = cfg_with_batch(65);
    CHECK(mockingbird_create(&c, &bad.e) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("batching: fewer wakeups, identical output") {
    const uint64_t unbatched = wakeups_after(0, 40);
    const uint64_t batched   = wakeups_after(8, 40);
    INFO("wakeups unbatched=" << unbatched << " batched=" << batched);
    CHECK(unbatched >= 20u);
    CHECK(batched * 2u < unbatched);
}

TEST_CASE("batching: live retune takes effect") {
    Engine eng; mockingbird_config_t c = cfg_with_batch(0);
    REQUIRE(mockingbird_create(&c, &eng.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(eng.e) == MOCKINGBIRD_OK);
    std::vector<int16_t> f(kFrame, 1000);
    auto run = [&](int frames, uint64_t& ts) {
        mockingbird_stats_t before{}; mockingbird_get_stats(eng.e, &before);
        for (int i = 0; i < frames; ++i) {
            REQUIRE(mockingbird_push_pcm(eng.e, f.data(), kFrame, ts) == MOCKINGBIRD_OK); ts += 20000;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        mockingbird_stats_t after{}; mockingbird_get_stats(eng.e, &after);
        return after.worker_wakeups - before.worker_wakeups;
    };
    uint64_t ts = 0;
    const uint64_t w0 = run(24, ts);
    c.worker_batch_frames = 8;
    REQUIRE(mockingbird_update_config(eng.e, &c) == MOCKINGBIRD_OK);
    const uint64_t w1 = run(24, ts);
    INFO("wakeups before retune=" << w0 << " after=" << w1);
    CHECK(w1 * 2u < w0);
}
