// tests/test_anomaly_reason.cpp
//
// Invariant: every MOCKINGBIRD_EVT_ANOMALY event MUST carry a non-zero reason_mask.
// An anomaly with no trigger bits is meaningless to the host; the whole point
// of the field is to say WHY the gate tripped (RMS / spectral flux / centroid).
//
// This is a regression test for a `reason=0` anomaly observed on-device. It
// drives the engine through a torture pattern designed to exercise the
// de-flicker "active hold" tail (where the detector coasts active for a few
// frames after a trip) interleaved with re-triggers, which is the path most
// likely to surface a reason mask that was cleared during the hold.

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
        if (e) { mockingbird_stop(e); mockingbird_destroy(e); }
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

void push_n(mockingbird_engine_t* e, const std::vector<int16_t>& f,
            uint32_t n, uint64_t& ts) {
    for (uint32_t i = 0; i < n; ++i) {
        REQUIRE(mockingbird_push_pcm(e, f.data(), 320, ts) == MOCKINGBIRD_OK);
        ts += 20'000ull;
        if ((i & 0x0Fu) == 0x0F) std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

}

TEST_CASE("Anomaly: every ANOMALY event carries a non-zero reason_mask") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend           = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec             = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.sample_rate_hz    = 16000;
    cfg.frame_ms          = 20;
    cfg.enable_vad        = 0;
    cfg.enable_anomaly    = 1;
    cfg.anomaly_rms_z         = 2.0f;
    cfg.anomaly_flux_z        = 2.0f;
    cfg.anomaly_background_ms = 2000;

    EngineGuard g;
    REQUIRE(mockingbird_create(&cfg, &g.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(g.e) == MOCKINGBIRD_OK);

    uint64_t ts = 0;
    const auto sil  = silence_frame();
    const auto loud = loud_tone(800.0, 0.9);

    uint32_t anomaly_total   = 0;
    uint32_t anomaly_zero    = 0;
    auto drain_events = [&]() {
        while (true) {
            mockingbird_event_t evt{};
            const mockingbird_status_t s = mockingbird_poll_event(g.e, &evt);
            if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) return;
            REQUIRE(s == MOCKINGBIRD_OK);
            if (evt.type == MOCKINGBIRD_EVT_ANOMALY) {
                ++anomaly_total;
                if (evt.reason_mask == 0) ++anomaly_zero;
            }
        }
    };

    push_n(g.e, sil, 100, ts);
    drain_events();

    for (int round = 0; round < 12; ++round) {
        push_n(g.e, loud, 4, ts);
        push_n(g.e, sil, (round % 3) + 2, ts);
        push_n(g.e, loud, 3, ts);
        push_n(g.e, sil, 10, ts);
        drain_events();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    drain_events();

    INFO("anomaly_total=" << anomaly_total << " anomaly_zero=" << anomaly_zero);
    CHECK(anomaly_total >= 1);
    CHECK(anomaly_zero == 0);
}
