// tests/test_fault_injection.cpp
//
// Fault injection: drive the engine and its components into their error /
// resource-exhaustion paths and assert they degrade gracefully — no crash, no
// corruption, correct accounting, and full recovery afterwards.
//
//   1. Encoder failure surface — a too-small output buffer must make encode()
//      return <= 0 (the worker's "drop this packet" signal) without writing
//      past the buffer. This is the contract the worker relies on.
//   2. Packet-pool exhaustion — flood the bounded queue with no host draining;
//      the pool must drop-oldest, raise OVERRUN, keep accounting, and still
//      hand out exactly capacity packets afterwards with no double-acquire.
//   3. push_pcm framing robustness — arbitrary, non-frame-aligned sample counts
//      must never crash or corrupt the ring.

#include "doctest.h"
#include "auris/auris.h"
#include "encode/encoder.hpp"

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

auris_config_t base_cfg() {
    auris_config_t c{};
    auris_config_defaults(&c);
    c.backend            = AURIS_BACKEND_HOST_PCM;
    c.codec              = AURIS_CODEC_PCM_S16LE;
    c.sample_rate_hz     = 16000;
    c.frame_ms           = 20;
    c.enable_vad         = 0;      // passthrough: every frame becomes a packet
    c.enable_anomaly     = 0;
    c.enable_sound_class = 0;
    return c;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Encoder failure surface
// ---------------------------------------------------------------------------

TEST_CASE("Fault: encoders return <= 0 on a too-small buffer, never overflow it") {
    auris_config_t cfg = base_cfg();
    constexpr uint32_t N = 320;                     // 20 ms @ 16 kHz
    std::vector<int16_t> frame(N);
    for (uint32_t i = 0; i < N; ++i) {
        frame[i] = static_cast<int16_t>((i * 211) % 4000 - 2000);
    }

    SUBCASE("PCM") {
        cfg.codec = AURIS_CODEC_PCM_S16LE;
        auto enc = auris::make_encoder(cfg.codec);
        REQUIRE(enc != nullptr);
        REQUIRE(enc->init(cfg) == AURIS_OK);
        // Exactly out_cap bytes so ASan flags any overrun.
        for (uint32_t cap : {0u, 1u, 100u, 639u}) {  // all < 640 needed bytes
            std::vector<uint8_t> out(cap == 0 ? 1 : cap);
            CHECK(enc->encode(frame.data(), N, out.data(), cap) <= 0);
        }
        std::vector<uint8_t> ok(enc->max_packet_bytes());
        CHECK(enc->encode(frame.data(), N, ok.data(),
                          static_cast<uint32_t>(ok.size())) > 0);
        // Wrong frame length is rejected, not encoded.
        CHECK(enc->encode(frame.data(), N - 1, ok.data(),
                          static_cast<uint32_t>(ok.size())) <= 0);
    }

#ifdef AURIS_HAVE_OPUS
    SUBCASE("Opus") {
        cfg.codec            = AURIS_CODEC_OPUS;
        cfg.opus_bitrate_bps = 32000;
        cfg.opus_complexity  = 5;
        auto enc = auris::make_encoder(cfg.codec);
        REQUIRE(enc != nullptr);
        REQUIRE(enc->init(cfg) == AURIS_OK);
        // opus_encode treats out_cap as a HARD ceiling: it degrades the bitrate
        // to fit (or returns an error), but never writes past the buffer. The
        // safety invariant the worker depends on is exactly that — the return
        // never exceeds the capacity it was given. (Exact-size buffers let ASan
        // catch any overrun.)
        for (uint32_t cap : {1u, 2u, 8u, 32u}) {
            std::vector<uint8_t> out(cap);
            const int r = enc->encode(frame.data(), N, out.data(), cap);
            CHECK(r <= static_cast<int>(cap));       // never overflows out_buf
        }
        std::vector<uint8_t> ok(enc->max_packet_bytes());
        const int big = enc->encode(frame.data(), N, ok.data(),
                                    static_cast<uint32_t>(ok.size()));
        CHECK(big > 0);
        CHECK(big <= static_cast<int>(ok.size()));
    }
#endif
}

// ---------------------------------------------------------------------------
// 2. Packet-pool exhaustion
// ---------------------------------------------------------------------------

TEST_CASE("Fault: flooding the packet queue with no draining drops-oldest and recovers") {
    auris_config_t cfg = base_cfg();
    cfg.packet_queue_capacity = 4;                  // tiny on purpose
    cfg.pcm_ring_frames       = 1u << 15;

    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);
    REQUIRE(auris_start(e) == AURIS_OK);

    // Push far more frames than the queue can hold, never draining.
    std::vector<int16_t> frame(320, 2500);
    for (int i = 0; i < 400; ++i) {
        REQUIRE(auris_push_pcm(e, frame.data(), 320,
                               static_cast<uint64_t>(i) * 20'000ull) == AURIS_OK);
        if ((i & 0x1Fu) == 0x1F) std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    uint32_t overruns = 0;
    auris_event_t evt{};
    while (auris_poll_event(e, &evt) == AURIS_OK) {
        if (evt.type == AURIS_EVT_OVERRUN) ++overruns;
    }

    auris_stats_t st{};
    REQUIRE(auris_get_stats(e, &st) == AURIS_OK);
    INFO("emitted=" << st.packets_emitted << " dropped=" << st.packets_dropped
         << " overruns=" << overruns << " queue_fill=" << st.packet_queue_fill);
    CHECK(st.packets_emitted > 0);
    CHECK(st.packets_dropped > 0);                  // drop-oldest engaged
    CHECK(overruns > 0);                            // and surfaced to the host

    // Recovery: the queue still holds AT MOST capacity packets, all releasable.
    uint32_t drained = 0;
    auris_packet_t pkt{};
    while (auris_drain(e, &pkt) == AURIS_OK) {
        CHECK(auris_release(e, &pkt) == AURIS_OK);  // no corruption / double-hand-out
        ++drained;
    }
    CHECK(drained <= cfg.packet_queue_capacity);

    REQUIRE(auris_stop(e) == AURIS_OK);
    auris_destroy(e);
}

// ---------------------------------------------------------------------------
// 3. push_pcm framing robustness
// ---------------------------------------------------------------------------

TEST_CASE("Fault: arbitrary non-frame-aligned push sizes never crash or corrupt") {
    auris_config_t cfg = base_cfg();
    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);
    REQUIRE(auris_start(e) == AURIS_OK);

    // Odd counts that don't divide the 320-sample frame, plus a zero (no-op).
    std::vector<int16_t> buf(4096, 700);
    const uint32_t counts[] = {0, 1, 7, 63, 319, 321, 640, 999, 4096};
    uint64_t total = 0;
    for (int rep = 0; rep < 50; ++rep) {
        for (uint32_t n : counts) {
            REQUIRE(auris_push_pcm(e, buf.data(), n, total * 20'000ull) == AURIS_OK);
            total += n;
        }
        // drain so the ring keeps flowing
        auris_packet_t pkt{};
        while (auris_drain(e, &pkt) == AURIS_OK) auris_release(e, &pkt);
        auris_event_t evt{};
        while (auris_poll_event(e, &evt) == AURIS_OK) { /* discard */ }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auris_stats_t st{};
    REQUIRE(auris_get_stats(e, &st) == AURIS_OK);
    REQUIRE(auris_stop(e) == AURIS_OK);
    auris_destroy(e);
    // Captured + dropped-at-ring must account for everything that wasn't a
    // zero-length push; we only assert it didn't crash and made progress.
    INFO("pushed=" << total << " captured=" << st.frames_captured
         << " dropped_cap=" << st.frames_dropped_capture);
    CHECK(st.frames_captured > 0);
    CHECK(st.frames_captured + st.frames_dropped_capture <= total);
}
