// tests/test_abi_validation.cpp
//
// Boundary-validation tests for the public C ABI. Every entry point must
// reject bad inputs with a status code (never crash, never leak).

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// --- Tier-1 hardening regressions (config bounds + push bounds) -------------

TEST_CASE("mockingbird_create rejects absurd duration fields (would OOM the worker)") {
    mockingbird_config_t base{};
    mockingbird_config_defaults(&base);
    base.backend = MOCKINGBIRD_BACKEND_HOST_PCM;
    mockingbird_engine_t* e = nullptr;

    SUBCASE("pre_roll_ms")           { base.pre_roll_ms = 0xFFFFFFFFu; }
    SUBCASE("post_roll_ms")          { base.post_roll_ms = 0xFFFFFFFFu; }
    SUBCASE("anomaly_background_ms") { base.anomaly_background_ms = 0xFFFFFFFFu; }
    SUBCASE("segment_max_ms")        { base.segment_max_ms = 0xFFFFFFFFu; }
    SUBCASE("vad_hangover_ms")       { base.vad_hangover_ms = 0xFFFFFFFFu; }

    CHECK(mockingbird_create(&base, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(e == nullptr);
}

TEST_CASE("mockingbird_create rejects non-finite / out-of-range float config") {
    mockingbird_config_t base{};
    mockingbird_config_defaults(&base);
    base.backend = MOCKINGBIRD_BACKEND_HOST_PCM; // anomaly on by default
    mockingbird_engine_t* e = nullptr;

    SUBCASE("NaN z-score")      { base.anomaly_rms_z  = std::nanf(""); }
    SUBCASE("infinite z-score") { base.anomaly_flux_z = INFINITY; }
    SUBCASE("negative z-score") { base.anomaly_rms_z  = -1.0f; }

    CHECK(mockingbird_create(&base, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(e == nullptr);
}

TEST_CASE("mockingbird_push_pcm rejects frame_count beyond ring capacity (no OOB read)") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend         = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.enable_vad      = 0;
    cfg.enable_anomaly  = 0;
    cfg.pcm_ring_frames = 1u << 14; // 16384 samples
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    std::vector<int16_t> buf(320, 0);
    CHECK(mockingbird_push_pcm(e, buf.data(), 1u << 20, 0) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_push_pcm(e, buf.data(), 320, 0) == MOCKINGBIRD_OK);

    mockingbird_stop(e);
    mockingbird_destroy(e);
}

TEST_CASE("mockingbird_config_defaults(NULL) returns INVALID_ARG") {
    CHECK(mockingbird_config_defaults(nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_config_defaults populates known fields") {
    mockingbird_config_t cfg{};
    REQUIRE(mockingbird_config_defaults(&cfg) == MOCKINGBIRD_OK);
    CHECK(cfg.sample_rate_hz == 16000);
    CHECK(cfg.frame_ms       == 20);
    CHECK(cfg.channels       == 1);
    CHECK(cfg.enable_vad     == 1);
    CHECK(cfg.pcm_ring_frames       > 0);
    CHECK(cfg.packet_queue_capacity > 0);
}

TEST_CASE("mockingbird_create rejects NULL config + NULL out") {
    mockingbird_engine_t* e = reinterpret_cast<mockingbird_engine_t*>(0xDEADBEEF);
    CHECK(mockingbird_create(nullptr, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(e == nullptr);

    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend = MOCKINGBIRD_BACKEND_HOST_PCM;
    CHECK(mockingbird_create(&cfg, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_create rejects out-of-set sample rate") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend        = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.sample_rate_hz = 12345;             // not in {8/16/32/48 kHz}
    mockingbird_engine_t* e = nullptr;
    CHECK(mockingbird_create(&cfg, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(e == nullptr);
}

TEST_CASE("mockingbird_create rejects out-of-set frame_ms") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend  = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.frame_ms = 25;                      // not in {10, 20, 30}
    mockingbird_engine_t* e = nullptr;
    CHECK(mockingbird_create(&cfg, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_create rejects channels != 1 (v1 is mono-only)") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend  = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.channels = 2;
    mockingbird_engine_t* e = nullptr;
    CHECK(mockingbird_create(&cfg, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_create rejects vad_aggressiveness out of [0,3]") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend           = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.vad_aggressiveness = 7;
    mockingbird_engine_t* e = nullptr;
    CHECK(mockingbird_create(&cfg, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_create rejects libopus bitrate out of nominal range") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend          = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec            = MOCKINGBIRD_CODEC_OPUS;
    cfg.opus_bitrate_bps = 100;             // below 6000
    mockingbird_engine_t* e = nullptr;
    CHECK(mockingbird_create(&cfg, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_destroy(NULL) is a no-op (no crash)") {
    mockingbird_destroy(nullptr);            // must not crash
    CHECK(true);
}

TEST_CASE("mockingbird_start/stop/push_pcm on NULL return INVALID_ARG") {
    CHECK(mockingbird_start(nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_stop(nullptr)  == MOCKINGBIRD_ERR_INVALID_ARG);
    int16_t s = 0;
    CHECK(mockingbird_push_pcm(nullptr, &s, 1, 0) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("mockingbird_status_str returns a non-empty string for every defined code") {
    const mockingbird_status_t all[] = {
        MOCKINGBIRD_OK,
        MOCKINGBIRD_ERR_INVALID_ARG,
        MOCKINGBIRD_ERR_INVALID_STATE,
        MOCKINGBIRD_ERR_NO_MEMORY,
        MOCKINGBIRD_ERR_BACKEND,
        MOCKINGBIRD_ERR_NOT_RUNNING,
        MOCKINGBIRD_ERR_ALREADY_RUNNING,
        MOCKINGBIRD_ERR_QUEUE_EMPTY,
        MOCKINGBIRD_ERR_PERMISSION,
        MOCKINGBIRD_ERR_UNSUPPORTED,
        MOCKINGBIRD_ERR_INTERNAL,
    };
    for (mockingbird_status_t s : all) {
        const char* str = mockingbird_status_str(static_cast<int>(s));
        REQUIRE(str != nullptr);
        CHECK(std::strlen(str) > 0);
    }
}

TEST_CASE("mockingbird_status_str returns MOCKINGBIRD_UNKNOWN for out-of-range ints") {
    // int parameter makes this safe: no enum-domain UB even for far-out
    // values.
    const char* unk = mockingbird_status_str(-9999);
    REQUIRE(unk != nullptr);
    CHECK(std::strcmp(unk, "MOCKINGBIRD_UNKNOWN") == 0);
    CHECK(std::strcmp(mockingbird_status_str(424242), "MOCKINGBIRD_UNKNOWN") == 0);
}

TEST_CASE("mockingbird_version_string is non-empty and stable across calls") {
    const char* v1 = mockingbird_version_string();
    const char* v2 = mockingbird_version_string();
    REQUIRE(v1 != nullptr);
    CHECK(std::strlen(v1) > 0);
    CHECK(v1 == v2);                     // owned by lib, same pointer
}
