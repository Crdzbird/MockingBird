// tests/test_silence_gate.cpp
//
// Silence gate (skip RNNoise / YAMNet on quiet input), worker-health stats,
// and adaptive degradation as seen through the public ABI on HOST_PCM.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Engine {
    mockingbird_engine_t* e = nullptr;
    ~Engine() { if (e) { mockingbird_stop(e); mockingbird_destroy(e); } }
};

constexpr uint32_t kFrame = 320;

mockingbird_config_t host_config() {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend        = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec          = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.enable_vad     = 0;
    cfg.enable_anomaly = 0;
    cfg.pcm_ring_frames = 1u << 17;
    cfg.packet_queue_capacity = 4096;
    return cfg;
}

std::vector<int16_t> square(int16_t amp) {
    std::vector<int16_t> v(kFrame);
    for (size_t i = 0; i < v.size(); ++i) v[i] = (i % 32 < 16) ? amp : static_cast<int16_t>(-amp);
    return v;
}

void push_frames(mockingbird_engine_t* e, const std::vector<int16_t>& f, size_t n,
                 uint64_t& ts, int pace_ms = 0) {
    for (size_t i = 0; i < n; ++i) {
        REQUIRE(mockingbird_push_pcm(e, f.data(), kFrame, ts) == MOCKINGBIRD_OK);
        ts += 20000;
        if (pace_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(pace_ms));
    }
    for (int spin = 0; spin < 500; ++spin) {
        mockingbird_stats_t s{}; mockingbird_get_stats(e, &s);
        if (s.pcm_ring_fill_frames == 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

mockingbird_stats_t stats(mockingbird_engine_t* e) { mockingbird_stats_t s{}; mockingbird_get_stats(e, &s); return s; }

#if defined(MOCKINGBIRD_HAVE_SOUND_CLASS)
std::string yamnet_path() { return std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/yamnet_3s.onnx"; }
#endif

}

TEST_CASE("gate: defaults, validation, and reserved stats are zero") {
    mockingbird_config_t cfg{}; mockingbird_config_defaults(&cfg);
    CHECK(cfg.silence_gate_dbfs == doctest::Approx(-60.0f));
    CHECK(cfg.enable_adaptive_degradation == 0);
    CHECK(cfg.degrade_busy_ppm == 700000u);
    CHECK(cfg.sound_class_interval_ms == 1000u);

    Engine bad;
    mockingbird_config_t c = host_config();
    c.silence_gate_dbfs = 1.0f;
    CHECK(mockingbird_create(&c, &bad.e) == MOCKINGBIRD_ERR_INVALID_ARG);
    c = host_config(); c.silence_gate_dbfs = NAN;
    CHECK(mockingbird_create(&c, &bad.e) == MOCKINGBIRD_ERR_INVALID_ARG);
    c = host_config(); c.degrade_busy_ppm = 1000001u;
    CHECK(mockingbird_create(&c, &bad.e) == MOCKINGBIRD_ERR_INVALID_ARG);

    Engine ok;
    c = host_config();
    REQUIRE(mockingbird_create(&c, &ok.e) == MOCKINGBIRD_OK);
    mockingbird_stats_t s{}; std::memset(&s, 0xFF, sizeof(s));
    REQUIRE(mockingbird_get_stats(ok.e, &s) == MOCKINGBIRD_OK);
    for (uint32_t r : s._reserved) CHECK(r == 0u);
    CHECK(s.degradation_level == 0u);
}

TEST_CASE("gate: worker health counters are populated") {
    Engine eng; mockingbird_config_t c = host_config();
    REQUIRE(mockingbird_create(&c, &eng.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(eng.e) == MOCKINGBIRD_OK);
    uint64_t ts = 0;
    push_frames(eng.e, square(3000), 60, ts, /*pace_ms=*/20);
    const auto s = stats(eng.e);
    CHECK(s.worker_wakeups > 0u);
    CHECK(s.worker_busy_ppm <= 1000000u);
    CHECK(s.frames_captured == 60u * kFrame);
    CHECK(s.degradation_level == 0u);
}

#if defined(MOCKINGBIRD_HAVE_RNNOISE)
TEST_CASE("gate: RNNoise is skipped on quiet frames and runs on loud ones") {
    Engine eng; mockingbird_config_t c = host_config();
    c.enable_rnnoise = 1;
    REQUIRE(mockingbird_create(&c, &eng.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(eng.e) == MOCKINGBIRD_OK);
    uint64_t ts = 0;
    push_frames(eng.e, square(0), 25, ts);
    CHECK(stats(eng.e).preproc_frames_skipped == 25u);
    push_frames(eng.e, square(3000), 25, ts);
    CHECK(stats(eng.e).preproc_frames_skipped == 25u);

    SUBCASE("live retune: disabling the gate stops the skipping") {
        mockingbird_config_t live = c; live.silence_gate_dbfs = -120.0f;
        REQUIRE(mockingbird_update_config(eng.e, &live) == MOCKINGBIRD_OK);
        push_frames(eng.e, square(0), 10, ts);
        CHECK(stats(eng.e).preproc_frames_skipped == 25u);
    }
}
#endif

#if defined(MOCKINGBIRD_HAVE_SOUND_CLASS)
TEST_CASE("gate: YAMNet skips inference on an all-quiet window, runs on sound") {
    Engine eng; mockingbird_config_t c = host_config();
    const std::string model = yamnet_path();
    c.enable_sound_class = 1; c.sound_class_model_path = model.c_str();
    c.sound_class_interval_ms = 500;
    REQUIRE(mockingbird_create(&c, &eng.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(eng.e) == MOCKINGBIRD_OK);
    uint64_t ts = 0;
    push_frames(eng.e, square(0), 250, ts);
    auto s = stats(eng.e);
    CHECK(s.sound_class_inferences == 0u);
    CHECK(s.sound_class_inferences_skipped > 0u);
    const auto skipped_after_silence = s.sound_class_inferences_skipped;

    push_frames(eng.e, square(6000), 100, ts);
    s = stats(eng.e);
    CHECK(s.sound_class_inferences > 0u);
    CHECK(s.sound_class_inferences_skipped == skipped_after_silence);
}

TEST_CASE("gate: disabled gate classifies silence too") {
    Engine eng; mockingbird_config_t c = host_config();
    const std::string model = yamnet_path();
    c.enable_sound_class = 1; c.sound_class_model_path = model.c_str();
    c.sound_class_interval_ms = 500;
    c.silence_gate_dbfs = -120.0f;
    REQUIRE(mockingbird_create(&c, &eng.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(eng.e) == MOCKINGBIRD_OK);
    uint64_t ts = 0;
    push_frames(eng.e, square(0), 250, ts);
    const auto s = stats(eng.e);
    CHECK(s.sound_class_inferences > 0u);
    CHECK(s.sound_class_inferences_skipped == 0u);
}

namespace {
struct EvtSink { std::vector<mockingbird_event_t> evts; };
void on_evt(const mockingbird_event_t* e, void* u) { static_cast<EvtSink*>(u)->evts.push_back(*e); }
}

TEST_CASE("degradation: sheds classifier work under load and reports it") {
    EvtSink sink;
    Engine eng; mockingbird_config_t c = host_config();
    const std::string model = yamnet_path();
    c.enable_sound_class = 1; c.sound_class_model_path = model.c_str();
    c.sound_class_interval_ms = 500;
    c.enable_adaptive_degradation = 1;
    c.degrade_busy_ppm = 1;
    REQUIRE(mockingbird_create(&c, &eng.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_set_event_callback(eng.e, on_evt, &sink) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(eng.e) == MOCKINGBIRD_OK);

    uint64_t ts = 0;
    push_frames(eng.e, square(6000), 120, ts, /*pace_ms=*/20);
    const auto s = stats(eng.e);
    CHECK(s.degradation_level >= 2u);

    uint32_t last_level = 0; size_t n_deg = 0;
    for (const auto& e : sink.evts) {
        if (e.type != MOCKINGBIRD_EVT_DEGRADATION) continue;
        ++n_deg;
        CHECK(e.reason_mask == last_level + 1);
        CHECK(e.confidence >= 0.0f); CHECK(e.confidence <= 1.0f);
        last_level = e.reason_mask;
    }
    CHECK(n_deg == s.degradation_level);
    CHECK(last_level == s.degradation_level);
}

TEST_CASE("degradation: never acts when disabled, even under 'load'") {
    EvtSink sink;
    Engine eng; mockingbird_config_t c = host_config();
    const std::string model = yamnet_path();
    c.enable_sound_class = 1; c.sound_class_model_path = model.c_str();
    c.enable_adaptive_degradation = 0;
    c.degrade_busy_ppm = 1;
    REQUIRE(mockingbird_create(&c, &eng.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_set_event_callback(eng.e, on_evt, &sink) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(eng.e) == MOCKINGBIRD_OK);
    uint64_t ts = 0;
    push_frames(eng.e, square(6000), 70, ts, /*pace_ms=*/20);
    CHECK(stats(eng.e).degradation_level == 0u);
    for (const auto& e : sink.evts) CHECK(e.type != MOCKINGBIRD_EVT_DEGRADATION);
}
#endif
