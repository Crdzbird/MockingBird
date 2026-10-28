// tests/test_opus_encoder.cpp
//
// Opus end-to-end through the public C ABI. Skipped if libopus is not
// compiled into this build (MOCKINGBIRD_HAVE_OPUS not defined).

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

#ifdef MOCKINGBIRD_HAVE_OPUS

namespace {
std::vector<int16_t> tone(uint32_t samples) {
    std::vector<int16_t> v(samples);
    for (uint32_t i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / 16000.0;
        v[i] = static_cast<int16_t>(
            std::lround(0.3 * 32767.0 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * t)));
    }
    return v;
}

}

TEST_CASE("Opus passthrough: packets carry CODEC_OPUS and are smaller than PCM") {
    mockingbird_config_t cfg{};
    REQUIRE(mockingbird_config_defaults(&cfg) == MOCKINGBIRD_OK);
    cfg.backend          = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec            = MOCKINGBIRD_CODEC_OPUS;
    cfg.sample_rate_hz   = 16000;
    cfg.frame_ms         = 20;
    cfg.opus_bitrate_bps = 24000;
    cfg.enable_vad       = 0;
    cfg.enable_anomaly   = 0;

    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    constexpr uint32_t kFrames = 25;
    const auto pcm = tone(320);
    for (uint32_t i = 0; i < kFrames; ++i) {
        REQUIRE(mockingbird_push_pcm(e, pcm.data(), 320, uint64_t{i} * 20'000ull) == MOCKINGBIRD_OK);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    uint32_t drained         = 0;
    uint32_t total_opus_bytes = 0;
    while (true) {
        mockingbird_packet_t pkt{};
        const mockingbird_status_t s = mockingbird_drain(e, &pkt);
        if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) break;
        REQUIRE(s == MOCKINGBIRD_OK);
        CHECK(pkt.codec == MOCKINGBIRD_CODEC_OPUS);
        CHECK(pkt.size > 0);
        CHECK(pkt.size <= 1275);
        total_opus_bytes += pkt.size;
        ++drained;
        REQUIRE(mockingbird_release(e, &pkt) == MOCKINGBIRD_OK);
    }

    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);

    CHECK(drained > 0);
    CHECK(total_opus_bytes < 8000);
}

TEST_CASE("Opus rejects 32 kHz: not a libopus sample rate") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend        = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec          = MOCKINGBIRD_CODEC_OPUS;
    cfg.sample_rate_hz = 32000;
    mockingbird_engine_t* e = nullptr;
    CHECK(mockingbird_create(&cfg, &e) == MOCKINGBIRD_ERR_UNSUPPORTED);
    CHECK(e == nullptr);
}

#else

TEST_CASE("Opus tests skipped: MOCKINGBIRD_HAVE_OPUS not defined in this build") {
    MESSAGE("libopus not linked, skip");
    CHECK(true);
}

#endif
