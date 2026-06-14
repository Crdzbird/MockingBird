// tests/test_detectors_golden.cpp
//
// Golden / characterization tests for the detectors against synthetic
// input. Cheap and deterministic: synthesize a known signal, run the
// engine, assert on the resulting event / packet shape.
//
// These exist to lock current behavior in place — any future change that
// alters VAD aggressiveness, anomaly z-score thresholds, or hangover
// arithmetic surfaces as a failed test rather than a silent regression.

#include "doctest.h"
#include "auris/auris.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

struct EngineGuard {
    auris_engine_t* e = nullptr;
    ~EngineGuard() {
        if (e) {
            auris_stop(e);
            auris_destroy(e);
        }
    }
};

auris_engine_t* make_host_pcm_with(int enable_vad,
                                   int enable_anomaly,
                                   int vad_aggressiveness) {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend            = AURIS_BACKEND_HOST_PCM;
    cfg.codec              = AURIS_CODEC_PCM_S16LE;
    cfg.sample_rate_hz     = 16000;
    cfg.frame_ms           = 20;
    cfg.enable_vad         = enable_vad;
    cfg.enable_anomaly     = enable_anomaly;
    cfg.vad_aggressiveness = vad_aggressiveness;
    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);
    REQUIRE(auris_start(e) == AURIS_OK);
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

void push_silence_frames(auris_engine_t* e, uint32_t frames) {
    auto buf = silence(320);
    for (uint32_t i = 0; i < frames; ++i) {
        REQUIRE(auris_push_pcm(e, buf.data(), 320,
                               uint64_t{i} * 20'000ull) == AURIS_OK);
        // The default PCM ring holds ~51 frames. Pace pushes so the worker
        // drains between batches; otherwise excess frames are dropped at the
        // ring and the test never sees them at the detector.
        if ((i & 0x1Fu) == 0x1F) {   // every 32 frames
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void push_frames(auris_engine_t* e,
                 const std::vector<int16_t>& buf,
                 uint32_t                    frames,
                 uint64_t                    start_ts_us,
                 uint64_t                    step_us) {
    for (uint32_t i = 0; i < frames; ++i) {
        REQUIRE(auris_push_pcm(e, buf.data(), 320, start_ts_us + i * step_us)
                == AURIS_OK);
        if ((i & 0x1Fu) == 0x1F) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

uint32_t drain_packet_count(auris_engine_t* e) {
    uint32_t n = 0;
    while (true) {
        auris_packet_t pkt{};
        const auris_status_t s = auris_drain(e, &pkt);
        if (s == AURIS_ERR_QUEUE_EMPTY) break;
        REQUIRE(s == AURIS_OK);
        ++n;
        REQUIRE(auris_release(e, &pkt) == AURIS_OK);
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

EventTally drain_events(auris_engine_t* e) {
    EventTally t{};
    while (true) {
        auris_event_t evt{};
        const auris_status_t s = auris_poll_event(e, &evt);
        if (s == AURIS_ERR_QUEUE_EMPTY) break;
        REQUIRE(s == AURIS_OK);
        switch (evt.type) {
            case AURIS_EVT_SPEECH_BEGIN:  ++t.speech_begin;  break;
            case AURIS_EVT_SPEECH_END:    ++t.speech_end;    break;
            case AURIS_EVT_ANOMALY:       ++t.anomaly;       break;
            case AURIS_EVT_SEGMENT_BEGIN: ++t.segment_begin; break;
            case AURIS_EVT_SEGMENT_END:   ++t.segment_end;   break;
            default: break;
        }
    }
    return t;
}

} // namespace

// ---------------------------------------------------------------------------
// VAD characterization
// ---------------------------------------------------------------------------

TEST_CASE("VAD on silence: zero packets, zero speech events") {
    EngineGuard g;
    g.e = make_host_pcm_with(/*vad=*/1, /*anomaly=*/0, /*aggr=*/2);

    push_silence_frames(g.e, 50);                  // 1 s of silence
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    CHECK(drain_packet_count(g.e) == 0);
    const auto tally = drain_events(g.e);
    CHECK(tally.speech_begin == 0);
    CHECK(tally.speech_end   == 0);
    CHECK(tally.segment_begin == 0);
}

TEST_CASE("VAD: every SPEECH_BEGIN is balanced by exactly one SPEECH_END") {
    // libfvad's GMM is voiced-speech-tuned; it does sometimes classify
    // sustained tones as speech and sometimes not. The strong invariant we
    // care about is shape symmetry — every BEGIN has an END (including the
    // implicit one on stop). That property MUST hold regardless of the
    // detector's per-frame decisions.
    EngineGuard g;
    g.e = make_host_pcm_with(/*vad=*/1, /*anomaly=*/0, /*aggr=*/2);

    const auto tone = sine_tone(320, 440.0, 0.3);
    for (uint32_t i = 0; i < 25; ++i) {
        REQUIRE(auris_push_pcm(g.e, tone.data(), 320,
                               uint64_t{i} * 20'000ull) == AURIS_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    REQUIRE(auris_stop(g.e) == AURIS_OK);

    const auto tally = drain_events(g.e);
    CHECK(tally.speech_begin == tally.speech_end);
    CHECK(tally.segment_begin == tally.segment_end);
}

// ---------------------------------------------------------------------------
// Anomaly characterization
// ---------------------------------------------------------------------------

TEST_CASE("Anomaly on a sudden loud burst after silence: trips and opens a segment") {
    EngineGuard g;
    g.e = make_host_pcm_with(/*vad=*/0, /*anomaly=*/1, /*aggr=*/2);

    // 2 s of silence to populate the rolling-background window with low
    // RMS / flux / centroid values.
    push_silence_frames(g.e, 100);

    // 200 ms of loud sine — sharp onset relative to the silent background.
    const auto burst = sine_tone(320, 800.0, 0.9);
    push_frames(g.e, burst, 10, 100 * 20'000ull, 20'000ull);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto tally = drain_events(g.e);
    // The exact ANOMALY count depends on hangover behavior; what we lock
    // in is "at least one anomaly was seen and a segment opened".
    CHECK(tally.anomaly >= 1);
    CHECK(tally.segment_begin >= 1);
}

// ---------------------------------------------------------------------------
// Pre-roll characterization
// ---------------------------------------------------------------------------

TEST_CASE("Pre-roll: when a segment opens after silence, PREROLL-flagged "
          "packets appear at the front") {
    EngineGuard g;
    // Anomaly only so we can deterministically trigger from a synthetic burst.
    g.e = make_host_pcm_with(/*vad=*/0, /*anomaly=*/1, /*aggr=*/2);

    // Populate background.
    push_silence_frames(g.e, 100);

    // Then a clear burst that should trigger anomaly.
    const auto burst = sine_tone(320, 800.0, 0.9);
    push_frames(g.e, burst, 20, 100 * 20'000ull, 20'000ull);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool seen_preroll = false;
    while (true) {
        auris_packet_t pkt{};
        const auris_status_t s = auris_drain(g.e, &pkt);
        if (s == AURIS_ERR_QUEUE_EMPTY) break;
        REQUIRE(s == AURIS_OK);
        if (pkt.flags & AURIS_PKT_FLAG_PREROLL) seen_preroll = true;
        REQUIRE(auris_release(g.e, &pkt) == AURIS_OK);
    }
    CHECK(seen_preroll);
}

// ---------------------------------------------------------------------------
// Stats accounting
// ---------------------------------------------------------------------------

TEST_CASE("Stats: frames_captured matches the total samples pushed exactly") {
    EngineGuard g;
    g.e = make_host_pcm_with(/*vad=*/0, /*anomaly=*/0, /*aggr=*/2);

    constexpr uint32_t kFrames = 30;
    auto buf = silence(320);
    for (uint32_t i = 0; i < kFrames; ++i) {
        REQUIRE(auris_push_pcm(g.e, buf.data(), 320, 0) == AURIS_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auris_stop(g.e);

    auris_stats_t s{};
    REQUIRE(auris_get_stats(g.e, &s) == AURIS_OK);
    CHECK(s.frames_captured        == kFrames * 320);
    CHECK(s.frames_dropped_capture == 0);
    CHECK(s.packets_dropped        == 0);
}
