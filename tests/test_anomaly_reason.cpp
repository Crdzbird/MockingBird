// tests/test_anomaly_reason.cpp
//
// Invariant: every AURIS_EVT_ANOMALY event MUST carry a non-zero reason_mask.
// An anomaly with no trigger bits is meaningless to the host — the whole point
// of the field is to say WHY the gate tripped (RMS / spectral flux / centroid).
//
// This is a regression test for a `reason=0` anomaly observed on-device. It
// drives the engine through a torture pattern designed to exercise the
// de-flicker "active hold" tail (where the detector coasts active for a few
// frames after a trip) interleaved with re-triggers, which is the path most
// likely to surface a reason mask that was cleared during the hold.

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
        if (e) { auris_stop(e); auris_destroy(e); }
    }
};

std::vector<int16_t> silence_frame() { return std::vector<int16_t>(320, 0); }

std::vector<int16_t> loud_tone(double freq_hz, double amp01) {
    std::vector<int16_t> v(320);
    for (uint32_t i = 0; i < 320; ++i) {
        const double t = static_cast<double>(i) / 16000.0;
        v[i] = static_cast<int16_t>(std::lround(
            amp01 * 32767.0 *
            std::sin(2.0 * 3.14159265358979323846 * freq_hz * t)));
    }
    return v;
}

void push_n(auris_engine_t* e, const std::vector<int16_t>& f,
            uint32_t n, uint64_t& ts) {
    for (uint32_t i = 0; i < n; ++i) {
        REQUIRE(auris_push_pcm(e, f.data(), 320, ts) == AURIS_OK);
        ts += 20'000ull;
        if ((i & 0x0Fu) == 0x0F) std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

} // namespace

TEST_CASE("Anomaly: every ANOMALY event carries a non-zero reason_mask") {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend           = AURIS_BACKEND_HOST_PCM;
    cfg.codec             = AURIS_CODEC_PCM_S16LE;
    cfg.sample_rate_hz    = 16000;
    cfg.frame_ms          = 20;
    cfg.enable_vad        = 0;     // anomaly only — deterministic
    cfg.enable_anomaly    = 1;
    // High sensitivity (low z thresholds) so small steps trip readily, the
    // same regime the on-device 60% dial put the gate in.
    cfg.anomaly_rms_z         = 2.0f;
    cfg.anomaly_flux_z        = 2.0f;
    cfg.anomaly_background_ms = 2000;

    EngineGuard g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    uint64_t ts = 0;
    const auto sil  = silence_frame();
    const auto loud = loud_tone(800.0, 0.9);

    uint32_t anomaly_total   = 0;
    uint32_t anomaly_zero    = 0;
    auto drain_events = [&]() {
        while (true) {
            auris_event_t evt{};
            const auris_status_t s = auris_poll_event(g.e, &evt);
            if (s == AURIS_ERR_QUEUE_EMPTY) return;
            REQUIRE(s == AURIS_OK);
            if (evt.type == AURIS_EVT_ANOMALY) {
                ++anomaly_total;
                if (evt.reason_mask == 0) ++anomaly_zero;
            }
        }
    };

    // Establish a quiet background.
    push_n(g.e, sil, 100, ts);
    drain_events();

    // Torture pattern: bursts separated by gaps of varying length, including
    // gaps SHORTER than the de-flicker hold (so the gate coasts active and the
    // internal reason mask is momentarily empty) and gaps LONGER than the hold
    // (so the gate fully re-arms and a fresh rising edge fires).
    for (int round = 0; round < 12; ++round) {
        push_n(g.e, loud, 4, ts);                 // burst (trip)
        push_n(g.e, sil, (round % 3) + 2, ts);    // short gap: 2..4 frames
        push_n(g.e, loud, 3, ts);                 // re-trip during/after hold
        push_n(g.e, sil, 10, ts);                 // long gap: hold expires
        drain_events();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    drain_events();

    INFO("anomaly_total=" << anomaly_total << " anomaly_zero=" << anomaly_zero);
    CHECK(anomaly_total >= 1);          // the pattern must actually trip
    CHECK(anomaly_zero == 0);           // and never with an empty reason mask
}
