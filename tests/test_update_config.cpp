// tests/test_update_config.cpp
//
// auris_update_config: retune a live engine without destroying it. Verifies the
// tunable subset applies, structural changes are rejected, and detector state
// survives a retune (the whole point — no model reload, no restart).

#include "doctest.h"
#include "auris/auris.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

auris_config_t base_config() {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend        = AURIS_BACKEND_HOST_PCM;
    cfg.codec          = AURIS_CODEC_PCM_S16LE;
    cfg.enable_vad     = 1;
    cfg.enable_anomaly = 1;
    return cfg;
}

struct Engine {
    auris_engine_t* e = nullptr;
    ~Engine() {
        if (e) { auris_stop(e); auris_destroy(e); }
    }
};

// Loud/quiet alternating frames so the anomaly gate has something to chew on.
std::vector<int16_t> tone(size_t n, int16_t amp) {
    std::vector<int16_t> v(n);
    for (size_t i = 0; i < n; ++i) {
        v[i] = static_cast<int16_t>((i % 32 < 16) ? amp : -amp);
    }
    return v;
}

} // namespace

TEST_CASE("auris_update_config rejects null args") {
    auris_config_t cfg = base_config();
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    CHECK(auris_update_config(nullptr, &cfg) == AURIS_ERR_INVALID_ARG);
    CHECK(auris_update_config(g.e, nullptr)  == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_update_config applies tunable fields on a running engine") {
    auris_config_t cfg = base_config();
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    // Every tunable knob, changed at once.
    auris_config_t up = cfg;
    up.vad_aggressiveness    = 0;
    up.vad_neural_threshold  = 0.25f;
    up.vad_hangover_ms       = 500;
    up.anomaly_rms_z         = 1.75f;
    up.anomaly_flux_z        = 1.75f;
    up.sound_class_threshold = 0.2f;
    CHECK(auris_update_config(g.e, &up) == AURIS_OK);

    // Engine keeps running and still accepts audio after the retune.
    const auto frame = tone(320, 6000);
    for (int i = 0; i < 10; ++i) {
        REQUIRE(auris_push_pcm(g.e, frame.data(), 320,
                               static_cast<uint64_t>(i) * 20'000ull) == AURIS_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auris_stats_t st{};
    REQUIRE(auris_get_stats(g.e, &st) == AURIS_OK);
    CHECK(st.frames_captured == 3200);
    CHECK(st.frames_dropped_capture == 0);
}

TEST_CASE("auris_update_config rejects structural changes") {
    auris_config_t cfg = base_config();
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    auris_config_t up = cfg;
    SUBCASE("sample rate")   { up.sample_rate_hz = 48000; }
    SUBCASE("frame_ms")      { up.frame_ms = 30; }
    SUBCASE("backend")       { up.backend = AURIS_BACKEND_MINIAUDIO; }
    SUBCASE("codec")         { up.codec = AURIS_CODEC_OPUS; }
    SUBCASE("ring size")     { up.pcm_ring_frames = 1u << 15; }
    SUBCASE("queue size")    { up.packet_queue_capacity = 512; }
    SUBCASE("pre_roll_ms")   { up.pre_roll_ms = 1000; }
    SUBCASE("background_ms") { up.anomaly_background_ms = 5000; }

    CHECK(auris_update_config(g.e, &up) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_update_config rejects an invalid config outright") {
    auris_config_t cfg = base_config();
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);

    auris_config_t up = cfg;
    SUBCASE("NaN z-score")     { up.anomaly_rms_z = std::nanf(""); }
    SUBCASE("negative z")      { up.anomaly_flux_z = -1.0f; }
    SUBCASE("bad aggressive")  { up.vad_aggressiveness = 9; }
    CHECK(auris_update_config(g.e, &up) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_update_config can toggle a detector off and back on") {
    auris_config_t cfg = base_config();
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    auris_config_t off = cfg;
    off.enable_anomaly = 0;
    CHECK(auris_update_config(g.e, &off) == AURIS_OK);

    auris_config_t on = cfg;   // anomaly back on — it was constructed at create
    on.enable_anomaly = 1;
    CHECK(auris_update_config(g.e, &on) == AURIS_OK);
}

TEST_CASE("auris_update_config cannot enable a detector that was off at create") {
    auris_config_t cfg = base_config();
    cfg.enable_anomaly = 0;          // never constructed
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    auris_config_t up = cfg;
    up.enable_anomaly = 1;
    CHECK(auris_update_config(g.e, &up) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_update_config preserves detector state (no restart)") {
    // The point of retuning in place: a segment already open must stay open and
    // keep the same id across the update — a destroy/recreate would reset it.
    auris_config_t cfg = base_config();
    cfg.enable_vad     = 0;          // passthrough: one always-on segment
    cfg.enable_anomaly = 0;
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    const auto frame = tone(320, 4000);
    for (int i = 0; i < 5; ++i) {
        REQUIRE(auris_push_pcm(g.e, frame.data(), 320,
                               static_cast<uint64_t>(i) * 20'000ull) == AURIS_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    uint64_t seg_before = 0;
    auris_packet_t pkt{};
    while (auris_drain(g.e, &pkt) == AURIS_OK) {
        seg_before = pkt.segment_id;
        auris_release(g.e, &pkt);
    }
    REQUIRE(seg_before != 0);

    auris_config_t up = cfg;
    up.vad_hangover_ms = 400;
    REQUIRE(auris_update_config(g.e, &up) == AURIS_OK);

    for (int i = 5; i < 10; ++i) {
        REQUIRE(auris_push_pcm(g.e, frame.data(), 320,
                               static_cast<uint64_t>(i) * 20'000ull) == AURIS_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    uint64_t seg_after = 0;
    while (auris_drain(g.e, &pkt) == AURIS_OK) {
        seg_after = pkt.segment_id;
        auris_release(g.e, &pkt);
    }
    REQUIRE(seg_after != 0);
    CHECK(seg_after == seg_before);   // same segment: the engine never restarted

    auris_stats_t st{};
    REQUIRE(auris_get_stats(g.e, &st) == AURIS_OK);
    CHECK(st.segments_opened == 1);
}

TEST_CASE("auris_update_config is safe under concurrent retune + push") {
    // Run with TSan: the worker snapshots tunables between frames while the
    // host hammers update_config from another thread.
    auris_config_t cfg = base_config();
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    std::atomic<bool> stop{false};
    std::thread tuner([&] {
        auris_config_t up = cfg;
        for (int i = 0; !stop.load(std::memory_order_relaxed) && i < 2000; ++i) {
            up.anomaly_rms_z = 1.5f + static_cast<float>(i % 40) * 0.1f;
            up.vad_aggressiveness = i % 4;
            auris_update_config(g.e, &up);
        }
    });

    const auto frame = tone(320, 5000);
    for (int i = 0; i < 200; ++i) {
        auris_push_pcm(g.e, frame.data(), 320,
                       static_cast<uint64_t>(i) * 20'000ull);
        auris_packet_t pkt{};
        while (auris_drain(g.e, &pkt) == AURIS_OK) auris_release(g.e, &pkt);
        auris_event_t evt{};
        while (auris_poll_event(g.e, &evt) == AURIS_OK) { /* discard */ }
    }
    stop.store(true, std::memory_order_relaxed);
    tuner.join();

    auris_stats_t st{};
    REQUIRE(auris_get_stats(g.e, &st) == AURIS_OK);
    CHECK(st.frames_captured > 0);
}
