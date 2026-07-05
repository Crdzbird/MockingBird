// tests/test_abi_validation.cpp
//
// Boundary-validation tests for the public C ABI. Every entry point must
// reject bad inputs with a status code (never crash, never leak).

#include "doctest.h"
#include "auris/auris.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// --- Tier-1 hardening regressions (config bounds + push bounds) -------------

TEST_CASE("auris_create rejects absurd duration fields (would OOM the worker)") {
    auris_config_t base{};
    auris_config_defaults(&base);
    base.backend = AURIS_BACKEND_HOST_PCM;
    auris_engine_t* e = nullptr;

    SUBCASE("pre_roll_ms")           { base.pre_roll_ms = 0xFFFFFFFFu; }
    SUBCASE("post_roll_ms")          { base.post_roll_ms = 0xFFFFFFFFu; }
    SUBCASE("anomaly_background_ms") { base.anomaly_background_ms = 0xFFFFFFFFu; }
    SUBCASE("segment_max_ms")        { base.segment_max_ms = 0xFFFFFFFFu; }
    SUBCASE("vad_hangover_ms")       { base.vad_hangover_ms = 0xFFFFFFFFu; }

    CHECK(auris_create(&base, &e) == AURIS_ERR_INVALID_ARG);
    CHECK(e == nullptr);
}

TEST_CASE("auris_create rejects non-finite / out-of-range float config") {
    auris_config_t base{};
    auris_config_defaults(&base);
    base.backend = AURIS_BACKEND_HOST_PCM; // anomaly on by default
    auris_engine_t* e = nullptr;

    SUBCASE("NaN z-score")      { base.anomaly_rms_z  = std::nanf(""); }
    SUBCASE("infinite z-score") { base.anomaly_flux_z = INFINITY; }
    SUBCASE("negative z-score") { base.anomaly_rms_z  = -1.0f; }

    CHECK(auris_create(&base, &e) == AURIS_ERR_INVALID_ARG);
    CHECK(e == nullptr);
}

TEST_CASE("auris_push_pcm rejects frame_count beyond ring capacity (no OOB read)") {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend         = AURIS_BACKEND_HOST_PCM;
    cfg.enable_vad      = 0;
    cfg.enable_anomaly  = 0;
    cfg.pcm_ring_frames = 1u << 14; // 16384 samples
    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);
    REQUIRE(auris_start(e) == AURIS_OK);

    std::vector<int16_t> buf(320, 0);
    CHECK(auris_push_pcm(e, buf.data(), 1u << 20, 0) == AURIS_ERR_INVALID_ARG);
    CHECK(auris_push_pcm(e, buf.data(), 320, 0) == AURIS_OK);

    auris_stop(e);
    auris_destroy(e);
}

TEST_CASE("auris_config_defaults(NULL) returns INVALID_ARG") {
    CHECK(auris_config_defaults(nullptr) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_config_defaults populates known fields") {
    auris_config_t cfg{};
    REQUIRE(auris_config_defaults(&cfg) == AURIS_OK);
    CHECK(cfg.sample_rate_hz == 16000);
    CHECK(cfg.frame_ms       == 20);
    CHECK(cfg.channels       == 1);
    CHECK(cfg.enable_vad     == 1);
    CHECK(cfg.pcm_ring_frames       > 0);
    CHECK(cfg.packet_queue_capacity > 0);
}

TEST_CASE("auris_create rejects NULL config + NULL out") {
    auris_engine_t* e = reinterpret_cast<auris_engine_t*>(0xDEADBEEF);
    CHECK(auris_create(nullptr, &e) == AURIS_ERR_INVALID_ARG);
    CHECK(e == nullptr);

    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend = AURIS_BACKEND_HOST_PCM;
    CHECK(auris_create(&cfg, nullptr) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_create rejects out-of-set sample rate") {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend        = AURIS_BACKEND_HOST_PCM;
    cfg.sample_rate_hz = 12345;             // not in {8/16/32/48 kHz}
    auris_engine_t* e = nullptr;
    CHECK(auris_create(&cfg, &e) == AURIS_ERR_INVALID_ARG);
    CHECK(e == nullptr);
}

TEST_CASE("auris_create rejects out-of-set frame_ms") {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend  = AURIS_BACKEND_HOST_PCM;
    cfg.frame_ms = 25;                      // not in {10, 20, 30}
    auris_engine_t* e = nullptr;
    CHECK(auris_create(&cfg, &e) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_create rejects channels != 1 (v1 is mono-only)") {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend  = AURIS_BACKEND_HOST_PCM;
    cfg.channels = 2;
    auris_engine_t* e = nullptr;
    CHECK(auris_create(&cfg, &e) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_create rejects vad_aggressiveness out of [0,3]") {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend           = AURIS_BACKEND_HOST_PCM;
    cfg.vad_aggressiveness = 7;
    auris_engine_t* e = nullptr;
    CHECK(auris_create(&cfg, &e) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_create rejects libopus bitrate out of nominal range") {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend          = AURIS_BACKEND_HOST_PCM;
    cfg.codec            = AURIS_CODEC_OPUS;
    cfg.opus_bitrate_bps = 100;             // below 6000
    auris_engine_t* e = nullptr;
    CHECK(auris_create(&cfg, &e) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_destroy(NULL) is a no-op (no crash)") {
    auris_destroy(nullptr);            // must not crash
    CHECK(true);
}

TEST_CASE("auris_start/stop/push_pcm on NULL return INVALID_ARG") {
    CHECK(auris_start(nullptr) == AURIS_ERR_INVALID_ARG);
    CHECK(auris_stop(nullptr)  == AURIS_ERR_INVALID_ARG);
    int16_t s = 0;
    CHECK(auris_push_pcm(nullptr, &s, 1, 0) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("auris_status_str returns a non-empty string for every defined code") {
    const auris_status_t all[] = {
        AURIS_OK,
        AURIS_ERR_INVALID_ARG,
        AURIS_ERR_INVALID_STATE,
        AURIS_ERR_NO_MEMORY,
        AURIS_ERR_BACKEND,
        AURIS_ERR_NOT_RUNNING,
        AURIS_ERR_ALREADY_RUNNING,
        AURIS_ERR_QUEUE_EMPTY,
        AURIS_ERR_PERMISSION,
        AURIS_ERR_UNSUPPORTED,
        AURIS_ERR_INTERNAL,
    };
    for (auris_status_t s : all) {
        const char* str = auris_status_str(static_cast<int>(s));
        REQUIRE(str != nullptr);
        CHECK(std::strlen(str) > 0);
    }
}

TEST_CASE("auris_status_str returns AURIS_UNKNOWN for out-of-range ints") {
    // int parameter makes this safe: no enum-domain UB even for far-out
    // values.
    const char* unk = auris_status_str(-9999);
    REQUIRE(unk != nullptr);
    CHECK(std::strcmp(unk, "AURIS_UNKNOWN") == 0);
    CHECK(std::strcmp(auris_status_str(424242), "AURIS_UNKNOWN") == 0);
}

TEST_CASE("auris_version_string is non-empty and stable across calls") {
    const char* v1 = auris_version_string();
    const char* v2 = auris_version_string();
    REQUIRE(v1 != nullptr);
    CHECK(std::strlen(v1) > 0);
    CHECK(v1 == v2);                     // owned by lib, same pointer
}
