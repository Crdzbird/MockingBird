// tests/test_opus_encoder.cpp
//
// Opus end-to-end through the public C ABI. Skipped if libopus is not
// compiled into this build (AURIS_HAVE_OPUS not defined).

#include "doctest.h"
#include "auris/auris.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

#ifdef AURIS_HAVE_OPUS

namespace {

// Synthesize a 440 Hz sine wave at 16 kHz — gives libopus something real to
// encode (silence often hits the DTX path and produces 0-byte packets).
std::vector<int16_t> tone(uint32_t samples) {
    std::vector<int16_t> v(samples);
    for (uint32_t i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / 16000.0;
        v[i] = static_cast<int16_t>(
            std::lround(0.3 * 32767.0 * std::sin(2.0 * 3.14159265358979323846 * 440.0 * t)));
    }
    return v;
}

} // namespace

TEST_CASE("Opus passthrough: packets carry CODEC_OPUS and are smaller than PCM") {
    auris_config_t cfg{};
    REQUIRE(auris_config_defaults(&cfg) == AURIS_OK);
    cfg.backend          = AURIS_BACKEND_HOST_PCM;
    cfg.codec            = AURIS_CODEC_OPUS;
    cfg.sample_rate_hz   = 16000;
    cfg.frame_ms         = 20;
    cfg.opus_bitrate_bps = 24000;       // ~60 bytes/packet at 50 Hz
    cfg.enable_vad       = 0;
    cfg.enable_anomaly   = 0;

    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);
    REQUIRE(auris_start(e) == AURIS_OK);

    constexpr uint32_t kFrames = 25;     // 0.5 s of audio
    const auto pcm = tone(320);
    for (uint32_t i = 0; i < kFrames; ++i) {
        REQUIRE(auris_push_pcm(e, pcm.data(), 320, uint64_t{i} * 20'000ull) == AURIS_OK);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    uint32_t drained         = 0;
    uint32_t total_opus_bytes = 0;
    while (true) {
        auris_packet_t pkt{};
        const auris_status_t s = auris_drain(e, &pkt);
        if (s == AURIS_ERR_QUEUE_EMPTY) break;
        REQUIRE(s == AURIS_OK);
        CHECK(pkt.codec == AURIS_CODEC_OPUS);
        CHECK(pkt.size > 0);
        CHECK(pkt.size <= 1275);          // Opus RFC 6716 nominal max
        total_opus_bytes += pkt.size;
        ++drained;
        REQUIRE(auris_release(e, &pkt) == AURIS_OK);
    }

    REQUIRE(auris_stop(e) == AURIS_OK);
    auris_destroy(e);

    CHECK(drained > 0);
    // 25 frames of PCM = 25 * 640 = 16000 bytes. Opus at 24 kbps for 0.5 s
    // is ~1500 bytes. Anything close to PCM size means encoding silently
    // bypassed.
    CHECK(total_opus_bytes < 8000);      // half of PCM is a safe ceiling
}

TEST_CASE("Opus rejects 32 kHz: not a libopus sample rate") {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend        = AURIS_BACKEND_HOST_PCM;
    cfg.codec          = AURIS_CODEC_OPUS;
    cfg.sample_rate_hz = 32000;          // valid for libfvad, NOT for Opus
    auris_engine_t* e = nullptr;
    CHECK(auris_create(&cfg, &e) == AURIS_ERR_UNSUPPORTED);
    CHECK(e == nullptr);
}

#else

TEST_CASE("Opus tests skipped: AURIS_HAVE_OPUS not defined in this build") {
    MESSAGE("libopus not linked — skip");
    CHECK(true);
}

#endif  // AURIS_HAVE_OPUS
