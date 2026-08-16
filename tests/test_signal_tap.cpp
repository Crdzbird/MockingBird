// tests/test_signal_tap.cpp
//
// Input level metrics (RMS / peak dBFS in stats) and the continuous PCM tap —
// the ungated stream a host needs to draw a waveform or archive everything
// while still receiving segment-gated packets.

#include "doctest.h"
#include "auris/auris.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

struct Engine {
    auris_engine_t* e = nullptr;
    ~Engine() {
        if (e) { auris_stop(e); auris_destroy(e); }
    }
};

auris_config_t tap_config(int enable_tap, uint32_t tap_frames = 0) {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend        = AURIS_BACKEND_HOST_PCM;
    cfg.codec          = AURIS_CODEC_PCM_S16LE;
    cfg.enable_vad     = 0;      // passthrough so packets flow too
    cfg.enable_anomaly = 0;
    cfg.enable_pcm_tap = enable_tap;
    cfg.pcm_tap_frames = tap_frames;
    return cfg;
}

// Square wave at a known amplitude: RMS == peak == amp, so dBFS is exact.
std::vector<int16_t> square(size_t n, int16_t amp) {
    std::vector<int16_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = (i % 2 == 0) ? amp : static_cast<int16_t>(-amp);
    return v;
}

float expected_dbfs(int16_t amp) {
    return static_cast<float>(20.0 * std::log10(static_cast<double>(amp) / 32768.0));
}

void push_frames(auris_engine_t* e, const std::vector<int16_t>& frame, int count) {
    for (int i = 0; i < count; ++i) {
        REQUIRE(auris_push_pcm(e, frame.data(), static_cast<uint32_t>(frame.size()),
                               static_cast<uint64_t>(i) * 20'000ull) == AURIS_OK);
    }
}

} // namespace

TEST_CASE("stats report input RMS / peak dBFS for a known signal") {
    auris_config_t cfg = tap_config(0);
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    // Before any audio, levels read 0 (documented initial value).
    auris_stats_t st{};
    REQUIRE(auris_get_stats(g.e, &st) == AURIS_OK);
    CHECK(st.input_rms_dbfs == 0.0f);

    const int16_t amp = 8192;                       // exactly -12.04 dBFS
    push_frames(g.e, square(320, amp), 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    REQUIRE(auris_get_stats(g.e, &st) == AURIS_OK);
    const float want = expected_dbfs(amp);
    INFO("rms=" << st.input_rms_dbfs << " peak=" << st.input_peak_dbfs
         << " want=" << want);
    CHECK(st.input_rms_dbfs  == doctest::Approx(want).epsilon(0.01));
    CHECK(st.input_peak_dbfs == doctest::Approx(want).epsilon(0.01));
}

TEST_CASE("input level floors at -120 dBFS on digital silence") {
    auris_config_t cfg = tap_config(0);
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    push_frames(g.e, std::vector<int16_t>(320, 0), 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auris_stats_t st{};
    REQUIRE(auris_get_stats(g.e, &st) == AURIS_OK);
    CHECK(st.input_rms_dbfs  == -120.0f);
    CHECK(st.input_peak_dbfs == -120.0f);
}

TEST_CASE("a louder signal reports a higher level") {
    auto level_of = [](int16_t amp) {
        auris_config_t cfg = tap_config(0);
        Engine g;
        REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
        REQUIRE(auris_start(g.e) == AURIS_OK);
        push_frames(g.e, square(320, amp), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auris_stats_t st{};
        REQUIRE(auris_get_stats(g.e, &st) == AURIS_OK);
        return st.input_rms_dbfs;
    };
    CHECK(level_of(16384) > level_of(1024));
}

TEST_CASE("auris_read_pcm returns UNSUPPORTED when the tap is disabled") {
    auris_config_t cfg = tap_config(0);
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    int16_t buf[64];
    uint32_t written = 123;
    CHECK(auris_read_pcm(g.e, buf, 64, &written) == AURIS_ERR_UNSUPPORTED);
    CHECK(written == 0);
}

TEST_CASE("auris_read_pcm rejects null arguments") {
    auris_config_t cfg = tap_config(1);
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    int16_t buf[8];
    uint32_t written = 0;
    CHECK(auris_read_pcm(nullptr, buf, 8, &written) == AURIS_ERR_INVALID_ARG);
    CHECK(auris_read_pcm(g.e, nullptr, 8, &written) == AURIS_ERR_INVALID_ARG);
    CHECK(auris_read_pcm(g.e, buf, 8, nullptr)      == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("PCM tap delivers the processed stream sample-for-sample") {
    auris_config_t cfg = tap_config(1, /*tap_frames*/ 1u << 15);
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    // Ramp so every sample is identifiable and order is verifiable.
    constexpr uint32_t kFrame  = 320;
    constexpr int      kFrames = 20;
    std::vector<int16_t> sent;
    for (int f = 0; f < kFrames; ++f) {
        std::vector<int16_t> frame(kFrame);
        for (uint32_t i = 0; i < kFrame; ++i) {
            frame[i] = static_cast<int16_t>((f * kFrame + i) % 10000);
        }
        sent.insert(sent.end(), frame.begin(), frame.end());
        REQUIRE(auris_push_pcm(g.e, frame.data(), kFrame,
                               static_cast<uint64_t>(f) * 20'000ull) == AURIS_OK);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::vector<int16_t> got;
    std::vector<int16_t> chunk(512);
    while (true) {
        uint32_t written = 0;
        REQUIRE(auris_read_pcm(g.e, chunk.data(),
                               static_cast<uint32_t>(chunk.size()), &written) == AURIS_OK);
        if (written == 0) break;
        got.insert(got.end(), chunk.begin(), chunk.begin() + written);
    }

    REQUIRE(got.size() == sent.size());
    CHECK(std::equal(got.begin(), got.end(), sent.begin()));

    auris_stats_t st{};
    REQUIRE(auris_get_stats(g.e, &st) == AURIS_OK);
    CHECK(st.tap_samples_dropped == 0);
}

TEST_CASE("PCM tap is UNGATED: audio flows even when no segment is open") {
    // VAD on with silence pushed: no segment opens, so NO packets are emitted —
    // but the tap must still deliver every frame. This is the whole point.
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend        = AURIS_BACKEND_HOST_PCM;
    cfg.codec          = AURIS_CODEC_PCM_S16LE;
    cfg.enable_vad     = 1;
    cfg.enable_anomaly = 0;
    cfg.enable_pcm_tap = 1;
    cfg.pcm_tap_frames = 1u << 15;
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    push_frames(g.e, std::vector<int16_t>(320, 0), 15);   // silence
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    uint32_t packets = 0;
    auris_packet_t pkt{};
    while (auris_drain(g.e, &pkt) == AURIS_OK) { ++packets; auris_release(g.e, &pkt); }

    size_t tapped = 0;
    std::vector<int16_t> chunk(1024);
    while (true) {
        uint32_t written = 0;
        REQUIRE(auris_read_pcm(g.e, chunk.data(),
                               static_cast<uint32_t>(chunk.size()), &written) == AURIS_OK);
        if (written == 0) break;
        tapped += written;
    }

    INFO("packets=" << packets << " tapped=" << tapped);
    CHECK(packets == 0);            // segment never opened
    CHECK(tapped  == 15u * 320u);   // ...but every sample reached the tap
}

TEST_CASE("PCM tap counts drops when the host never drains") {
    auris_config_t cfg = tap_config(1, /*tap_frames*/ 512);  // tiny on purpose
    Engine g;
    REQUIRE(auris_create(&cfg, &g.e) == AURIS_OK);
    REQUIRE(auris_start(g.e) == AURIS_OK);

    push_frames(g.e, square(320, 4000), 40);   // 12800 samples into a 512 ring
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auris_stats_t st{};
    REQUIRE(auris_get_stats(g.e, &st) == AURIS_OK);
    INFO("tap_samples_dropped=" << st.tap_samples_dropped);
    CHECK(st.tap_samples_dropped > 0);   // overflow is reported, not hidden
    CHECK(st.frames_dropped_capture == 0); // and the capture path is unaffected
}
