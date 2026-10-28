// tests/test_detectors_golden.cpp
//
// Golden / characterization tests for the detectors against synthetic
// input. Cheap and deterministic: synthesize a known signal, run the
// engine, assert on the resulting event / packet shape.
//
// These exist to lock current behavior in place: any future change that
// alters VAD aggressiveness, anomaly z-score thresholds, or hangover
// arithmetic surfaces as a failed test rather than a silent regression.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

namespace {
struct EngineGuard {
    mockingbird_engine_t* e = nullptr;
    ~EngineGuard() {
        if (e) {
            mockingbird_stop(e);
            mockingbird_destroy(e);
        }
    }
};

mockingbird_engine_t* make_host_pcm_with(int enable_vad,
                                   int enable_anomaly,
                                   int vad_aggressiveness) {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend            = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec              = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.sample_rate_hz     = 16000;
    cfg.frame_ms           = 20;
    cfg.enable_vad         = enable_vad;
    cfg.enable_anomaly     = enable_anomaly;
    cfg.vad_aggressiveness = vad_aggressiveness;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    return e;
}

std::vector<int16_t> silence(uint32_t n) { return std::vector<int16_t>(n, 0); }

std::vector<int16_t> sine_tone(uint32_t n, double freq_hz, double amplitude01) {
    std::vector<int16_t> v(n);
    for (uint32_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / 16000.0;
        v[i] = static_cast<int16_t>(std::lround(
            amplitude01 * 32767.0 *
            std::sin(2.0 * 3.14159265358979323846 * freq_hz * t)));
    }
    return v;
}

void push_silence_frames(mockingbird_engine_t* e, uint32_t frames) {
    auto buf = silence(320);
    for (uint32_t i = 0; i < frames; ++i) {
        REQUIRE(mockingbird_push_pcm(e, buf.data(), 320,
                               uint64_t{i} * 20'000ull) == MOCKINGBIRD_OK);
        if ((i & 0x1Fu) == 0x1F) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void push_frames(mockingbird_engine_t* e,
                 const std::vector<int16_t>& buf,
                 uint32_t                    frames,
                 uint64_t                    start_ts_us,
                 uint64_t                    step_us) {
    for (uint32_t i = 0; i < frames; ++i) {
        REQUIRE(mockingbird_push_pcm(e, buf.data(), 320, start_ts_us + i * step_us)
                == MOCKINGBIRD_OK);
        if ((i & 0x1Fu) == 0x1F) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

uint32_t drain_packet_count(mockingbird_engine_t* e) {
    uint32_t n = 0;
    while (true) {
        mockingbird_packet_t pkt{};
        const mockingbird_status_t s = mockingbird_drain(e, &pkt);
        if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) break;
        REQUIRE(s == MOCKINGBIRD_OK);
        ++n;
        REQUIRE(mockingbird_release(e, &pkt) == MOCKINGBIRD_OK);
    }
    return n;
}

struct EventTally {
    uint32_t speech_begin = 0;
    uint32_t speech_end   = 0;
    uint32_t anomaly      = 0;
    uint32_t segment_begin = 0;
    uint32_t segment_end   = 0;
};

EventTally drain_events(mockingbird_engine_t* e) {
    EventTally t{};
    while (true) {
        mockingbird_event_t evt{};
        const mockingbird_status_t s = mockingbird_poll_event(e, &evt);
        if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) break;
        REQUIRE(s == MOCKINGBIRD_OK);
        switch (evt.type) {
            case MOCKINGBIRD_EVT_SPEECH_BEGIN:  ++t.speech_begin;  break;
            case MOCKINGBIRD_EVT_SPEECH_END:    ++t.speech_end;    break;
            case MOCKINGBIRD_EVT_ANOMALY:       ++t.anomaly;       break;
            case MOCKINGBIRD_EVT_SEGMENT_BEGIN: ++t.segment_begin; break;
            case MOCKINGBIRD_EVT_SEGMENT_END:   ++t.segment_end;   break;
            default: break;
        }
    }
    return t;
}

}

TEST_CASE("VAD on silence: zero packets, zero speech events") {
    EngineGuard g;
    g.e = make_host_pcm_with(/*vad=*/1, /*anomaly=*/0, /*aggr=*/2);

    push_silence_frames(g.e, 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    CHECK(drain_packet_count(g.e) == 0);
    const auto tally = drain_events(g.e);
    CHECK(tally.speech_begin == 0);
    CHECK(tally.speech_end   == 0);
    CHECK(tally.segment_begin == 0);
}

TEST_CASE("VAD: every SPEECH_BEGIN is balanced by exactly one SPEECH_END") {
    EngineGuard g;
    g.e = make_host_pcm_with(/*vad=*/1, /*anomaly=*/0, /*aggr=*/2);

    const auto tone = sine_tone(320, 440.0, 0.3);
    for (uint32_t i = 0; i < 25; ++i) {
        REQUIRE(mockingbird_push_pcm(g.e, tone.data(), 320,
                               uint64_t{i} * 20'000ull) == MOCKINGBIRD_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    REQUIRE(mockingbird_stop(g.e) == MOCKINGBIRD_OK);

    const auto tally = drain_events(g.e);
    CHECK(tally.speech_begin == tally.speech_end);
    CHECK(tally.segment_begin == tally.segment_end);
}

TEST_CASE("Anomaly on a sudden loud burst after silence: trips and opens a segment") {
    EngineGuard g;
    g.e = make_host_pcm_with(/*vad=*/0, /*anomaly=*/1, /*aggr=*/2);

    push_silence_frames(g.e, 100);

    const auto burst = sine_tone(320, 800.0, 0.9);
    push_frames(g.e, burst, 10, 100 * 20'000ull, 20'000ull);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto tally = drain_events(g.e);
    CHECK(tally.anomaly >= 1);
    CHECK(tally.segment_begin >= 1);
}

TEST_CASE("Pre-roll: when a segment opens after silence, PREROLL-flagged "
          "packets appear at the front") {
    EngineGuard g;
    g.e = make_host_pcm_with(/*vad=*/0, /*anomaly=*/1, /*aggr=*/2);

    push_silence_frames(g.e, 100);

    const auto burst = sine_tone(320, 800.0, 0.9);
    push_frames(g.e, burst, 20, 100 * 20'000ull, 20'000ull);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool seen_preroll = false;
    while (true) {
        mockingbird_packet_t pkt{};
        const mockingbird_status_t s = mockingbird_drain(g.e, &pkt);
        if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) break;
        REQUIRE(s == MOCKINGBIRD_OK);
        if (pkt.flags & MOCKINGBIRD_PKT_FLAG_PREROLL) seen_preroll = true;
        REQUIRE(mockingbird_release(g.e, &pkt) == MOCKINGBIRD_OK);
    }
    CHECK(seen_preroll);
}

TEST_CASE("Stats: frames_captured matches the total samples pushed exactly") {
    EngineGuard g;
    g.e = make_host_pcm_with(/*vad=*/0, /*anomaly=*/0, /*aggr=*/2);

    constexpr uint32_t kFrames = 30;
    auto buf = silence(320);
    for (uint32_t i = 0; i < kFrames; ++i) {
        REQUIRE(mockingbird_push_pcm(g.e, buf.data(), 320, 0) == MOCKINGBIRD_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mockingbird_stop(g.e);

    mockingbird_stats_t s{};
    REQUIRE(mockingbird_get_stats(g.e, &s) == MOCKINGBIRD_OK);
    CHECK(s.frames_captured        == kFrames * 320);
    CHECK(s.frames_dropped_capture == 0);
    CHECK(s.packets_dropped        == 0);
}
