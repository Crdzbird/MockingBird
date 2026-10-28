// tests/test_neural_vad.cpp
//
// Silero VAD via ONNX Runtime, differential against libfvad FAST on the
// same hello.wav fixture used by test_real_voice. Both detectors must
// recognise real speech; we verify the shape invariants for NEURAL match
// what we already established for FAST.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef MOCKINGBIRD_HAVE_NEURAL_VAD

namespace {
void set_model_env(const std::string& path) {
#if defined(_WIN32)
    _putenv_s("MOCKINGBIRD_NEURAL_VAD_MODEL", path.c_str());
#else
    setenv("MOCKINGBIRD_NEURAL_VAD_MODEL", path.c_str(), 1);
#endif
}

struct WavInts16 {
    std::vector<int16_t> samples;
    uint32_t             sample_rate_hz = 0;
};

WavInts16 read_wav_int16(const std::string& path) {
    WavInts16 out;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return out;
    auto read = [&](void* dst, size_t n) -> bool {
        return std::fread(dst, 1, n, f) == n;
    };
    char riff[4]; uint32_t total_size; char wave[4];
    if (!read(riff, 4) || std::memcmp(riff, "RIFF", 4) != 0) { std::fclose(f); return out; }
    if (!read(&total_size, 4)) { std::fclose(f); return out; }
    if (!read(wave, 4) || std::memcmp(wave, "WAVE", 4) != 0) { std::fclose(f); return out; }
    uint16_t audio_format = 0, channels = 0, bits_per_samp = 0;
    uint32_t sample_rate = 0;
    bool fmt_seen = false;
    while (true) {
        char chunk_id[4]; uint32_t chunk_size;
        if (!read(chunk_id, 4) || !read(&chunk_size, 4)) break;
        if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
            std::vector<uint8_t> buf(chunk_size);
            if (!read(buf.data(), chunk_size)) break;
            audio_format  = static_cast<uint16_t>(buf[0]  | (buf[1]  << 8));
            channels      = static_cast<uint16_t>(buf[2]  | (buf[3]  << 8));
            sample_rate   = static_cast<uint32_t>(buf[4]  | (buf[5]  << 8) |
                                                  (buf[6]  << 16) | (buf[7]  << 24));
            bits_per_samp = static_cast<uint16_t>(buf[14] | (buf[15] << 8));
            fmt_seen = true;
        } else if (std::memcmp(chunk_id, "data", 4) == 0) {
            if (!fmt_seen || audio_format != 1 || bits_per_samp != 16 || channels != 1) break;
            out.sample_rate_hz = sample_rate;
            const size_t sample_count = chunk_size / sizeof(int16_t);
            out.samples.resize(sample_count);
            if (!read(out.samples.data(), chunk_size)) out.samples.clear();
            break;
        } else {
            if (chunk_size & 1u) ++chunk_size;
            if (std::fseek(f, static_cast<long>(chunk_size), SEEK_CUR) != 0) break;
        }
    }
    std::fclose(f);
    return out;
}

struct VadResult {
    uint32_t speech_begin = 0;
    uint32_t speech_end   = 0;
    uint32_t segment_begin = 0;
    uint32_t segment_end   = 0;
    uint32_t packets       = 0;
    uint64_t frames_captured = 0;
    uint64_t frames_dropped  = 0;
};

VadResult run_vad_through_fixture(const WavInts16& wav, mockingbird_vad_tier_t tier) {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend            = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec              = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.sample_rate_hz     = 16000;
    cfg.frame_ms           = 20;
    cfg.enable_vad         = 1;
    cfg.enable_anomaly     = 0;
    cfg.vad_tier           = tier;
    cfg.vad_aggressiveness = 1;
    cfg.vad_neural_threshold = 0.5f;
    cfg.pcm_ring_frames = 1u << 16;

    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    constexpr uint32_t kFrameSamples = 320;
    const size_t total_frames = wav.samples.size() / kFrameSamples;
    VadResult vr{};

    auto drain_events = [&]() {
        while (true) {
            mockingbird_event_t evt{};
            const mockingbird_status_t s = mockingbird_poll_event(e, &evt);
            if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) return;
            REQUIRE(s == MOCKINGBIRD_OK);
            if (evt.type == MOCKINGBIRD_EVT_SPEECH_BEGIN)  ++vr.speech_begin;
            if (evt.type == MOCKINGBIRD_EVT_SPEECH_END)    ++vr.speech_end;
            if (evt.type == MOCKINGBIRD_EVT_SEGMENT_BEGIN) ++vr.segment_begin;
            if (evt.type == MOCKINGBIRD_EVT_SEGMENT_END)   ++vr.segment_end;
        }
    };
    auto drain_packets = [&]() {
        while (true) {
            mockingbird_packet_t pkt{};
            const mockingbird_status_t s = mockingbird_drain(e, &pkt);
            if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) return;
            REQUIRE(s == MOCKINGBIRD_OK);
            ++vr.packets;
            REQUIRE(mockingbird_release(e, &pkt) == MOCKINGBIRD_OK);
        }
    };

    for (size_t i = 0; i < total_frames; ++i) {
        const int16_t* p = wav.samples.data() + i * kFrameSamples;
        REQUIRE(mockingbird_push_pcm(e, p, kFrameSamples,
                               static_cast<uint64_t>(i) * 20'000ull) == MOCKINGBIRD_OK);
        if ((i & 0x1Fu) == 0x1F) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            drain_events();
            drain_packets();
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    drain_events();
    drain_packets();
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    drain_events();

    mockingbird_stats_t stats{};
    mockingbird_get_stats(e, &stats);
    vr.frames_captured = stats.frames_captured;
    vr.frames_dropped  = stats.frames_dropped_capture;
    mockingbird_destroy(e);
    return vr;
}

bool neural_vad_available() {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend  = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec    = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.vad_tier = MOCKINGBIRD_VAD_NEURAL;
    mockingbird_engine_t* e = nullptr;
    const mockingbird_status_t s = mockingbird_create(&cfg, &e);
    if (s != MOCKINGBIRD_OK) return false;
    mockingbird_destroy(e);
    return true;
}

}

TEST_CASE("Loading a model opts the process out of ONNX Runtime telemetry") {
    const bool preset = std::getenv("ORT_DISABLE_TELEMETRY") != nullptr;
    set_model_env(std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/silero_vad.onnx");
    const auto wav = read_wav_int16(std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/hello.wav");
    REQUIRE(wav.samples.size() > 0);
    run_vad_through_fixture(wav, MOCKINGBIRD_VAD_NEURAL);
    const char* v = std::getenv("ORT_DISABLE_TELEMETRY");
    REQUIRE(v != nullptr);
    if (!preset) CHECK(std::string(v) == "1");
}

TEST_CASE("Neural VAD: model loads when MOCKINGBIRD_NEURAL_VAD_MODEL points at the fixture") {
    set_model_env(std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/silero_vad.onnx");
    CHECK(neural_vad_available());
}

TEST_CASE("Neural VAD: detects speech in hello.wav, balanced events") {
    set_model_env(std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/silero_vad.onnx");
    const auto wav = read_wav_int16(
        std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/hello.wav");
    REQUIRE(wav.samples.size() > 0);
    REQUIRE(wav.sample_rate_hz == 16000);

    const auto neural = run_vad_through_fixture(wav, MOCKINGBIRD_VAD_NEURAL);

    INFO("NEURAL speech_begin=" << neural.speech_begin
         << " speech_end="     << neural.speech_end
         << " packets="        << neural.packets
         << " captured="       << neural.frames_captured
         << " dropped="        << neural.frames_dropped);

    CHECK(neural.speech_begin >= 1);
    CHECK(neural.speech_end >= neural.speech_begin - 1u);
    CHECK(neural.speech_end <= neural.speech_begin);
    CHECK(neural.segment_begin == neural.segment_end);
    CHECK(neural.frames_dropped == 0);
}

TEST_CASE("Neural VAD: 8 kHz uses the 256-sample Silero window (detects speech)") {
    set_model_env(std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/silero_vad.onnx");
    const auto wav16 = read_wav_int16(
        std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/hello.wav");
    REQUIRE(wav16.samples.size() > 0);
    REQUIRE(wav16.sample_rate_hz == 16000);

    std::vector<int16_t> s8;
    s8.reserve(wav16.samples.size() / 2);
    for (size_t i = 0; i + 1 < wav16.samples.size(); i += 2) {
        s8.push_back(static_cast<int16_t>(
            (static_cast<int32_t>(wav16.samples[i]) + wav16.samples[i + 1]) / 2));
    }

    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend              = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec                = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.sample_rate_hz       = 8000;
    cfg.frame_ms             = 20;
    cfg.enable_vad           = 1;
    cfg.enable_anomaly       = 0;
    cfg.vad_tier             = MOCKINGBIRD_VAD_NEURAL;
    cfg.vad_neural_threshold = 0.5f;
    cfg.pcm_ring_frames      = 1u << 16;

    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    constexpr uint32_t kFrame = 160;
    uint32_t speech_begin = 0;
    auto drain = [&]() {
        mockingbird_event_t evt{};
        while (mockingbird_poll_event(e, &evt) == MOCKINGBIRD_OK) {
            if (evt.type == MOCKINGBIRD_EVT_SPEECH_BEGIN) ++speech_begin;
        }
        mockingbird_packet_t pkt{};
        while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) mockingbird_release(e, &pkt);
    };

    const size_t frames = s8.size() / kFrame;
    for (size_t i = 0; i < frames; ++i) {
        REQUIRE(mockingbird_push_pcm(e, s8.data() + i * kFrame, kFrame,
                               static_cast<uint64_t>(i) * 20'000ull) == MOCKINGBIRD_OK);
        if ((i & 0x1Fu) == 0x1F) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            drain();
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    drain();
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    drain();
    mockingbird_destroy(e);

    INFO("8 kHz NEURAL speech_begin=" << speech_begin);
    CHECK(speech_begin >= 1);
}

TEST_CASE("Neural VAD vs FAST VAD: differential on hello.wav, both detect speech") {
    set_model_env(std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/silero_vad.onnx");
    const auto wav = read_wav_int16(
        std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/hello.wav");
    REQUIRE(wav.samples.size() > 0);

    const auto fast   = run_vad_through_fixture(wav, MOCKINGBIRD_VAD_FAST);
    const auto neural = run_vad_through_fixture(wav, MOCKINGBIRD_VAD_NEURAL);

    INFO("FAST   begin/end=" << fast.speech_begin   << "/" << fast.speech_end
         << " packets="      << fast.packets);
    INFO("NEURAL begin/end=" << neural.speech_begin << "/" << neural.speech_end
         << " packets="      << neural.packets);

    CHECK(fast.speech_begin   >= 1);
    CHECK(neural.speech_begin >= 1);
}

#else

TEST_CASE("Neural VAD tests skipped: MOCKINGBIRD_HAVE_NEURAL_VAD not defined in this build") {
    MESSAGE("ONNX Runtime not linked, skip");
    CHECK(true);
}

#endif
