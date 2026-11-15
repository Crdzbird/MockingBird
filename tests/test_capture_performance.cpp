// tests/test_capture_performance.cpp
//
// capture_performance defaults to DEFAULT, is validated, and reaches the
// capture backend. The capture gap estimate counts audio a backend never
// delivered (a stalled DSP, a late callback) without counting the bursts
// and early chunks of normal jitter, and pushed PCM never counts.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "capture/capture_backend.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace {
struct CaptureState {
    mockingbird::PcmCallback          cb          = nullptr;
    void*                             user        = nullptr;
    mockingbird_capture_performance_t performance = MOCKINGBIRD_CAPTURE_PERFORMANCE_DEFAULT;
    int                               inits       = 0;
};
CaptureState g_capture;

class ProbeBackend final : public mockingbird::CaptureBackend {
public:
    mockingbird_status_t init(const mockingbird_config_t& cfg, mockingbird::PcmCallback cb,
                              void* user) override {
        g_capture.cb          = cb;
        g_capture.user        = user;
        g_capture.performance = cfg.capture_performance;
        ++g_capture.inits;
        return MOCKINGBIRD_OK;
    }
    mockingbird_status_t start() override { return MOCKINGBIRD_OK; }
    mockingbird_status_t stop() override { return MOCKINGBIRD_OK; }
};

std::unique_ptr<mockingbird::CaptureBackend> probe_factory(mockingbird_backend_t) {
    return std::make_unique<ProbeBackend>();
}

struct Guard {
    Guard() {
        g_capture = CaptureState{};
        mockingbird::set_backend_factory_override(&probe_factory);
    }
    ~Guard() { mockingbird::set_backend_factory_override(nullptr); }
};

mockingbird_config_t capture_config(mockingbird_capture_performance_t performance) {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend             = MOCKINGBIRD_BACKEND_MINIAUDIO;
    cfg.enable_vad          = 0;
    cfg.enable_anomaly      = 0;
    cfg.capture_performance = performance;
    return cfg;
}

const std::vector<int16_t>& chunk() {
    static const std::vector<int16_t> samples(320, 50);
    return samples;
}

void deliver(uint64_t ts_us) { g_capture.cb(g_capture.user, chunk().data(), 320, ts_us); }

mockingbird_stats_t stats_of(mockingbird_engine_t* e) {
    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK);
    return st;
}

}

TEST_CASE("capture_performance defaults to DEFAULT and the ring holds two seconds at 16 kHz") {
    mockingbird_config_t cfg{};
    REQUIRE(mockingbird_config_defaults(&cfg) == MOCKINGBIRD_OK);
    CHECK(cfg.capture_performance == MOCKINGBIRD_CAPTURE_PERFORMANCE_DEFAULT);
    CHECK(cfg.pcm_ring_frames == 1u << 15);
    for (uint32_t r : cfg._reserved) CHECK(r == 0u);
}

TEST_CASE("capture_performance is validated and reaches the backend") {
    Guard g;
    for (auto p : {MOCKINGBIRD_CAPTURE_PERFORMANCE_DEFAULT, MOCKINGBIRD_CAPTURE_PERFORMANCE_LOW_LATENCY,
                   MOCKINGBIRD_CAPTURE_PERFORMANCE_POWER_SAVING}) {
        const mockingbird_config_t cfg = capture_config(p);
        mockingbird_engine_t* e = nullptr;
        REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
        REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
        CHECK(g_capture.performance == p);
        REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
        mockingbird_destroy(e);
    }
    mockingbird_config_t bad = capture_config(MOCKINGBIRD_CAPTURE_PERFORMANCE_DEFAULT);
    bad.capture_performance = static_cast<mockingbird_capture_performance_t>(3);
    mockingbird_engine_t* e = nullptr;
    CHECK(mockingbird_create(&bad, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(e == nullptr);
}

TEST_CASE("steady capture with jitter and bursts counts no gap") {
    Guard g;
    const mockingbird_config_t cfg = capture_config(MOCKINGBIRD_CAPTURE_PERFORMANCE_DEFAULT);
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    uint64_t ts = 5'000'000;
    for (int i = 0; i < 100; ++i) {
        deliver(ts + static_cast<uint64_t>((i % 3) * 7'000));
        ts += 20'000;
    }
    for (int burst = 0; burst < 20; ++burst) {
        const uint64_t late = ts + 150'000;
        for (int i = 0; i < 8; ++i) deliver(late);
        ts += 8 * 20'000;
    }
    const mockingbird_stats_t st = stats_of(e);
    CHECK(st.capture_gaps == 0u);
    CHECK(st.capture_gap_ms == 0u);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}

TEST_CASE("audio the platform never delivered counts as a capture gap") {
    Guard g;
    const mockingbird_config_t cfg = capture_config(MOCKINGBIRD_CAPTURE_PERFORMANCE_LOW_LATENCY);
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    uint64_t ts = 1'000'000;
    for (int i = 0; i < 50; ++i, ts += 20'000) deliver(ts);
    ts += 500'000;
    for (int i = 0; i < 50; ++i, ts += 20'000) deliver(ts);
    ts += 300'000;
    for (int i = 0; i < 10; ++i, ts += 20'000) deliver(ts);
    mockingbird_stats_t st = stats_of(e);
    CHECK(st.capture_gaps == 2u);
    CHECK(st.capture_gap_ms >= 795u);
    CHECK(st.capture_gap_ms <= 805u);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);

    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    for (int i = 0; i < 10; ++i, ts += 20'000) deliver(ts + 60'000'000);
    st = stats_of(e);
    CHECK(st.capture_gaps == 2u);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}

TEST_CASE("pushed PCM never counts a capture gap") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend        = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.enable_vad     = 0;
    cfg.enable_anomaly = 0;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    uint64_t ts = 0;
    for (int i = 0; i < 20; ++i, ts += 900'000) {
        REQUIRE(mockingbird_push_pcm(e, chunk().data(), 320, ts) == MOCKINGBIRD_OK);
    }
    const mockingbird_stats_t st = stats_of(e);
    CHECK(st.capture_gaps == 0u);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}
