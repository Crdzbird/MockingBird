// tests/test_update_config.cpp
//
// mockingbird_update_config: retune a live engine without destroying it. Verifies the
// tunable subset applies, structural changes are rejected, and detector state
// survives a retune (the whole point: no model reload, no restart).

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

namespace {
mockingbird_config_t base_config() {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend        = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec          = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.enable_vad     = 1;
    cfg.enable_anomaly = 1;
    return cfg;
}

struct Engine {
    mockingbird_engine_t* e = nullptr;
    ~Engine() {
        if (e) { mockingbird_stop(e); mockingbird_destroy(e); }
    }
};

std::vector<int16_t> tone(size_t n, int16_t amp) {
    std::vector<int16_t> v(n);
    for (size_t i = 0; i < n; ++i) {
        v[i] = static_cast<int16_t>((i % 32 < 16) ? amp : -amp);
    }
    return v;
}

}

TEST_CASE("mockingbird_update_config rejects null args") {
    mockingbird_config_t cfg = base_config();
    Engine g;
    REQUIRE(mockingbird_create(&cfg, &g.e) == MOCKINGBIRD_OK);
    CHECK(mockingbird_update_config(nullptr, &cfg) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_update_config(g.e, nullptr)  == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_update_config applies tunable fields on a running engine") {
    mockingbird_config_t cfg = base_config();
    Engine g;
    REQUIRE(mockingbird_create(&cfg, &g.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(g.e) == MOCKINGBIRD_OK);

    mockingbird_config_t up = cfg;
    up.vad_aggressiveness    = 0;
    up.vad_neural_threshold  = 0.25f;
    up.vad_hangover_ms       = 500;
    up.anomaly_rms_z         = 1.75f;
    up.anomaly_flux_z        = 1.75f;
    up.sound_class_threshold = 0.2f;
    CHECK(mockingbird_update_config(g.e, &up) == MOCKINGBIRD_OK);

    const auto frame = tone(320, 6000);
    for (int i = 0; i < 10; ++i) {
        REQUIRE(mockingbird_push_pcm(g.e, frame.data(), 320,
                               static_cast<uint64_t>(i) * 20'000ull) == MOCKINGBIRD_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(g.e, &st) == MOCKINGBIRD_OK);
    CHECK(st.frames_captured == 3200);
    CHECK(st.frames_dropped_capture == 0);
}

TEST_CASE("mockingbird_update_config rejects structural changes") {
    mockingbird_config_t cfg = base_config();
    Engine g;
    REQUIRE(mockingbird_create(&cfg, &g.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(g.e) == MOCKINGBIRD_OK);

    mockingbird_config_t up = cfg;
    SUBCASE("sample rate")   { up.sample_rate_hz = 48000; }
    SUBCASE("frame_ms")      { up.frame_ms = 30; }
    SUBCASE("backend")       { up.backend = MOCKINGBIRD_BACKEND_MINIAUDIO; }
    SUBCASE("codec")         { up.codec = MOCKINGBIRD_CODEC_OPUS; }
    SUBCASE("ring size")     { up.pcm_ring_frames = 1u << 15; }
    SUBCASE("queue size")    { up.packet_queue_capacity = 512; }
    SUBCASE("pre_roll_ms")   { up.pre_roll_ms = 1000; }
    SUBCASE("background_ms") { up.anomaly_background_ms = 5000; }

    CHECK(mockingbird_update_config(g.e, &up) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_update_config rejects an invalid config outright") {
    mockingbird_config_t cfg = base_config();
    Engine g;
    REQUIRE(mockingbird_create(&cfg, &g.e) == MOCKINGBIRD_OK);

    mockingbird_config_t up = cfg;
    SUBCASE("NaN z-score")     { up.anomaly_rms_z = std::nanf(""); }
    SUBCASE("negative z")      { up.anomaly_flux_z = -1.0f; }
    SUBCASE("bad aggressive")  { up.vad_aggressiveness = 9; }
    CHECK(mockingbird_update_config(g.e, &up) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_update_config can toggle a detector off and back on") {
    mockingbird_config_t cfg = base_config();
    Engine g;
    REQUIRE(mockingbird_create(&cfg, &g.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(g.e) == MOCKINGBIRD_OK);

    mockingbird_config_t off = cfg;
    off.enable_anomaly = 0;
    CHECK(mockingbird_update_config(g.e, &off) == MOCKINGBIRD_OK);

    mockingbird_config_t on = cfg;
    on.enable_anomaly = 1;
    CHECK(mockingbird_update_config(g.e, &on) == MOCKINGBIRD_OK);
}

TEST_CASE("mockingbird_update_config cannot enable a detector that was off at create") {
    mockingbird_config_t cfg = base_config();
    cfg.enable_anomaly = 0;
    Engine g;
    REQUIRE(mockingbird_create(&cfg, &g.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(g.e) == MOCKINGBIRD_OK);

    mockingbird_config_t up = cfg;
    up.enable_anomaly = 1;
    CHECK(mockingbird_update_config(g.e, &up) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_update_config preserves detector state (no restart)") {
    mockingbird_config_t cfg = base_config();
    cfg.enable_vad     = 0;
    cfg.enable_anomaly = 0;
    Engine g;
    REQUIRE(mockingbird_create(&cfg, &g.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(g.e) == MOCKINGBIRD_OK);

    const auto frame = tone(320, 4000);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(mockingbird_push_pcm(g.e, frame.data(), 320,
                               static_cast<uint64_t>(i) * 20'000ull) == MOCKINGBIRD_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    uint64_t seg_before = 0;
    mockingbird_packet_t pkt{};
    while (mockingbird_drain(g.e, &pkt) == MOCKINGBIRD_OK) {
        seg_before = pkt.segment_id;
        mockingbird_release(g.e, &pkt);
    }
    REQUIRE(seg_before != 0);

    mockingbird_config_t up = cfg;
    up.vad_hangover_ms = 400;
    REQUIRE(mockingbird_update_config(g.e, &up) == MOCKINGBIRD_OK);

    for (int i = 5; i < 10; ++i) {
        REQUIRE(mockingbird_push_pcm(g.e, frame.data(), 320,
                               static_cast<uint64_t>(i) * 20'000ull) == MOCKINGBIRD_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    uint64_t seg_after = 0;
    while (mockingbird_drain(g.e, &pkt) == MOCKINGBIRD_OK) {
        seg_after = pkt.segment_id;
        mockingbird_release(g.e, &pkt);
    }
    REQUIRE(seg_after != 0);
    CHECK(seg_after == seg_before);

    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(g.e, &st) == MOCKINGBIRD_OK);
    CHECK(st.segments_opened == 1);
}

TEST_CASE("mockingbird_update_config is safe under concurrent retune + push") {
    mockingbird_config_t cfg = base_config();
    Engine g;
    REQUIRE(mockingbird_create(&cfg, &g.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(g.e) == MOCKINGBIRD_OK);

    std::atomic<bool> stop{false};
    std::thread tuner([&] {
        mockingbird_config_t up = cfg;
        for (int i = 0; !stop.load(std::memory_order_relaxed) && i < 2000; ++i) {
            up.anomaly_rms_z = 1.5f + static_cast<float>(i % 40) * 0.1f;
            up.vad_aggressiveness = i % 4;
            mockingbird_update_config(g.e, &up);
        }
    });

    const auto frame = tone(320, 5000);
    for (int i = 0; i < 200; ++i) {
        mockingbird_push_pcm(g.e, frame.data(), 320,
                       static_cast<uint64_t>(i) * 20'000ull);
        mockingbird_packet_t pkt{};
        while (mockingbird_drain(g.e, &pkt) == MOCKINGBIRD_OK) mockingbird_release(g.e, &pkt);
        mockingbird_event_t evt{};
        while (mockingbird_poll_event(g.e, &evt) == MOCKINGBIRD_OK) { /* discard */ }
    }
    stop.store(true, std::memory_order_relaxed);
    tuner.join();

    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(g.e, &st) == MOCKINGBIRD_OK);
    CHECK(st.frames_captured > 0);
}
