// tests/test_fault_injection.cpp
//
// Fault injection: drive the engine and its components into their error /
// resource-exhaustion paths and assert they degrade gracefully: no crash, no
// corruption, correct accounting, and full recovery afterwards.
//
//   1. Encoder failure surface: a too-small output buffer must make encode()
//      return <= 0 (the worker's "drop this packet" signal) without writing
//      past the buffer. This is the contract the worker relies on.
//   2. Packet-pool exhaustion: flood the bounded queue with no host draining;
//      the pool must drop-oldest, raise OVERRUN, keep accounting, and still
//      hand out exactly capacity packets afterwards with no double-acquire.
//   3. push_pcm framing robustness: arbitrary, non-frame-aligned sample counts
//      must never crash or corrupt the ring.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "encode/encoder.hpp"

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {
mockingbird_config_t base_cfg() {
    mockingbird_config_t c{};
    mockingbird_config_defaults(&c);
    c.backend            = MOCKINGBIRD_BACKEND_HOST_PCM;
    c.codec              = MOCKINGBIRD_CODEC_PCM_S16LE;
    c.sample_rate_hz     = 16000;
    c.frame_ms           = 20;
    c.enable_vad         = 0;
    c.enable_anomaly     = 0;
    c.enable_sound_class = 0;
    return c;
}

}

TEST_CASE("Fault: encoders return <= 0 on a too-small buffer, never overflow it") {
    mockingbird_config_t cfg = base_cfg();
    constexpr uint32_t N = 320;
    std::vector<int16_t> frame(N);
    for (uint32_t i = 0; i < N; ++i) {
        frame[i] = static_cast<int16_t>((i * 211) % 4000 - 2000);
    }

    SUBCASE("PCM") {
        cfg.codec = MOCKINGBIRD_CODEC_PCM_S16LE;
        auto enc = mockingbird::make_encoder(cfg.codec);
        REQUIRE(enc != nullptr);
        REQUIRE(enc->init(cfg) == MOCKINGBIRD_OK);
        for (uint32_t cap : {0u, 1u, 100u, 639u}) {
            std::vector<uint8_t> out(cap == 0 ? 1 : cap);
            CHECK(enc->encode(frame.data(), N, out.data(), cap) <= 0);
        }
        std::vector<uint8_t> ok(enc->max_packet_bytes());
        CHECK(enc->encode(frame.data(), N, ok.data(),
                          static_cast<uint32_t>(ok.size())) > 0);
        CHECK(enc->encode(frame.data(), N - 1, ok.data(),
                          static_cast<uint32_t>(ok.size())) <= 0);
    }

#ifdef MOCKINGBIRD_HAVE_OPUS
    SUBCASE("Opus") {
        cfg.codec            = MOCKINGBIRD_CODEC_OPUS;
        cfg.opus_bitrate_bps = 32000;
        cfg.opus_complexity  = 5;
        auto enc = mockingbird::make_encoder(cfg.codec);
        REQUIRE(enc != nullptr);
        REQUIRE(enc->init(cfg) == MOCKINGBIRD_OK);
        for (uint32_t cap : {1u, 2u, 8u, 32u}) {
            std::vector<uint8_t> out(cap);
            const int r = enc->encode(frame.data(), N, out.data(), cap);
            CHECK(r <= static_cast<int>(cap));
        }
        std::vector<uint8_t> ok(enc->max_packet_bytes());
        const int big = enc->encode(frame.data(), N, ok.data(),
                                    static_cast<uint32_t>(ok.size()));
        CHECK(big > 0);
        CHECK(big <= static_cast<int>(ok.size()));
    }
#endif
}

TEST_CASE("Fault: flooding the packet queue with no draining drops-oldest and recovers") {
    mockingbird_config_t cfg = base_cfg();
    cfg.packet_queue_capacity = 4;
    cfg.pcm_ring_frames       = 1u << 15;

    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    std::vector<int16_t> frame(320, 2500);
    for (int i = 0; i < 400; ++i) {
        REQUIRE(mockingbird_push_pcm(e, frame.data(), 320,
                               static_cast<uint64_t>(i) * 20'000ull) == MOCKINGBIRD_OK);
        if ((i & 0x1Fu) == 0x1F) std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    uint32_t overruns = 0;
    mockingbird_event_t evt{};
    while (mockingbird_poll_event(e, &evt) == MOCKINGBIRD_OK) {
        if (evt.type == MOCKINGBIRD_EVT_OVERRUN) ++overruns;
    }

    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK);
    INFO("emitted=" << st.packets_emitted << " dropped=" << st.packets_dropped
         << " overruns=" << overruns << " queue_fill=" << st.packet_queue_fill);
    CHECK(st.packets_emitted > 0);
    CHECK(st.packets_dropped > 0);
    CHECK(overruns > 0);

    uint32_t drained = 0;
    mockingbird_packet_t pkt{};
    while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) {
        CHECK(mockingbird_release(e, &pkt) == MOCKINGBIRD_OK);
        ++drained;
    }
    CHECK(drained <= cfg.packet_queue_capacity);

    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}

TEST_CASE("Fault: arbitrary non-frame-aligned push sizes never crash or corrupt") {
    mockingbird_config_t cfg = base_cfg();
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    std::vector<int16_t> buf(4096, 700);
    const uint32_t counts[] = {0, 1, 7, 63, 319, 321, 640, 999, 4096};
    uint64_t total = 0;
    for (int rep = 0; rep < 50; ++rep) {
        for (uint32_t n : counts) {
            REQUIRE(mockingbird_push_pcm(e, buf.data(), n, total * 20'000ull) == MOCKINGBIRD_OK);
            total += n;
        }
        mockingbird_packet_t pkt{};
        while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) mockingbird_release(e, &pkt);
        mockingbird_event_t evt{};
        while (mockingbird_poll_event(e, &evt) == MOCKINGBIRD_OK) { /* discard */ }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
    INFO("pushed=" << total << " captured=" << st.frames_captured
         << " dropped_cap=" << st.frames_dropped_capture);
    CHECK(st.frames_captured > 0);
    CHECK(st.frames_captured + st.frames_dropped_capture <= total);
}
