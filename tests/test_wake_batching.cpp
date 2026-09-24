// tests/test_wake_batching.cpp
//
// worker_batch_frames: the capture thread wakes the worker only once a batch
// is queued. Same frames, same output, far fewer wakeups.

#include "doctest.h"
#include "auris/auris.h"

#include <chrono>
#include <thread>
#include <vector>

namespace {
struct Engine { auris_engine_t* e = nullptr; ~Engine() { if (e) { auris_stop(e); auris_destroy(e); } } };
constexpr uint32_t kFrame = 320;

auris_config_t cfg_with_batch(uint32_t batch) {
    auris_config_t c{}; auris_config_defaults(&c);
    c.backend = AURIS_BACKEND_HOST_PCM; c.codec = AURIS_CODEC_PCM_S16LE;
    c.enable_vad = 0; c.enable_anomaly = 0;
    c.worker_batch_frames = batch;
    return c;
}

// Paced pushes so the worker parks between frames when unbatched.
uint64_t wakeups_after(uint32_t batch, int frames) {
    Engine eng; auris_config_t c = cfg_with_batch(batch);
    REQUIRE(auris_create(&c, &eng.e) == AURIS_OK);
    REQUIRE(auris_start(eng.e) == AURIS_OK);
    std::vector<int16_t> f(kFrame, 1000);
    for (int i = 0; i < frames; ++i) {
        REQUIRE(auris_push_pcm(eng.e, f.data(), kFrame, static_cast<uint64_t>(i) * 20000) == AURIS_OK);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auris_stats_t s{}; auris_get_stats(eng.e, &s);
    // Every frame is still processed (auris_stop drains the tail).
    auris_stop(eng.e); auris_get_stats(eng.e, &s);
    CHECK(s.frames_captured == static_cast<uint64_t>(frames) * kFrame);
    CHECK(s.packets_emitted == static_cast<uint64_t>(frames));
    return s.worker_wakeups;
}
} // namespace

TEST_CASE("batching: validation and defaults") {
    auris_config_t c{}; auris_config_defaults(&c);
    CHECK(c.worker_batch_frames == 0u);
    Engine bad; c = cfg_with_batch(65);
    CHECK(auris_create(&c, &bad.e) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("batching: fewer wakeups, identical output") {
    const uint64_t unbatched = wakeups_after(0, 40);
    const uint64_t batched   = wakeups_after(8, 40);
    INFO("wakeups unbatched=" << unbatched << " batched=" << batched);
    CHECK(unbatched >= 20u);            // paced pushes: roughly one wake per frame
    CHECK(batched * 2u < unbatched);    // at most ~one wake per 8 frames
}

TEST_CASE("batching: live retune takes effect") {
    Engine eng; auris_config_t c = cfg_with_batch(0);
    REQUIRE(auris_create(&c, &eng.e) == AURIS_OK);
    REQUIRE(auris_start(eng.e) == AURIS_OK);
    std::vector<int16_t> f(kFrame, 1000);
    auto run = [&](int frames, uint64_t& ts) {
        auris_stats_t before{}; auris_get_stats(eng.e, &before);
        for (int i = 0; i < frames; ++i) {
            REQUIRE(auris_push_pcm(eng.e, f.data(), kFrame, ts) == AURIS_OK); ts += 20000;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auris_stats_t after{}; auris_get_stats(eng.e, &after);
        return after.worker_wakeups - before.worker_wakeups;
    };
    uint64_t ts = 0;
    const uint64_t w0 = run(24, ts);
    c.worker_batch_frames = 8;
    REQUIRE(auris_update_config(eng.e, &c) == AURIS_OK);
    const uint64_t w1 = run(24, ts);
    INFO("wakeups before retune=" << w0 << " after=" << w1);
    CHECK(w1 * 2u < w0);
}
