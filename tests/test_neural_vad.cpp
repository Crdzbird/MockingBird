// tests/test_neural_vad.cpp
//
// Silero VAD via ONNX Runtime, differential against libfvad FAST on the
// same hello.wav fixture used by test_real_voice. Both detectors must
// recognise real speech; we verify the shape invariants for NEURAL match
// what we already established for FAST.

#include "doctest.h"
#include "auris/auris.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef AURIS_HAVE_NEURAL_VAD

namespace {

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

VadResult run_vad_through_fixture(const WavInts16& wav, auris_vad_tier_t tier) {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend            = AURIS_BACKEND_HOST_PCM;
    cfg.codec              = AURIS_CODEC_PCM_S16LE;
    cfg.sample_rate_hz     = 16000;
    cfg.frame_ms           = 20;
    cfg.enable_vad         = 1;
    cfg.enable_anomaly     = 0;
    cfg.vad_tier           = tier;
    cfg.vad_aggressiveness = 1;
    cfg.vad_neural_threshold = 0.5f;
    // Silero adds ~5-10 ms of inference per frame; bump the PCM ring so the
    // test's batched pushes do not overflow while inference catches up.
    cfg.pcm_ring_frames = 1u << 16;       // 65536 samples = 4 s @ 16 kHz

    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);
    REQUIRE(auris_start(e) == AURIS_OK);

    constexpr uint32_t kFrameSamples = 320;
    const size_t total_frames = wav.samples.size() / kFrameSamples;
    VadResult vr{};

    auto drain_events = [&]() {
        while (true) {
            auris_event_t evt{};
            const auris_status_t s = auris_poll_event(e, &evt);
            if (s == AURIS_ERR_QUEUE_EMPTY) return;
            REQUIRE(s == AURIS_OK);
            if (evt.type == AURIS_EVT_SPEECH_BEGIN)  ++vr.speech_begin;
            if (evt.type == AURIS_EVT_SPEECH_END)    ++vr.speech_end;
            if (evt.type == AURIS_EVT_SEGMENT_BEGIN) ++vr.segment_begin;
            if (evt.type == AURIS_EVT_SEGMENT_END)   ++vr.segment_end;
        }
    };
    auto drain_packets = [&]() {
        while (true) {
            auris_packet_t pkt{};
            const auris_status_t s = auris_drain(e, &pkt);
            if (s == AURIS_ERR_QUEUE_EMPTY) return;
            REQUIRE(s == AURIS_OK);
            ++vr.packets;
            REQUIRE(auris_release(e, &pkt) == AURIS_OK);
        }
    };

    for (size_t i = 0; i < total_frames; ++i) {
        const int16_t* p = wav.samples.data() + i * kFrameSamples;
        REQUIRE(auris_push_pcm(e, p, kFrameSamples,
                               static_cast<uint64_t>(i) * 20'000ull) == AURIS_OK);
        if ((i & 0x1Fu) == 0x1F) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            drain_events();
            drain_packets();
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    drain_events();
    drain_packets();
    REQUIRE(auris_stop(e) == AURIS_OK);
    drain_events();

    auris_stats_t stats{};
    auris_get_stats(e, &stats);
    vr.frames_captured = stats.frames_captured;
    vr.frames_dropped  = stats.frames_dropped_capture;
    auris_destroy(e);
    return vr;
}

bool neural_vad_available() {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend  = AURIS_BACKEND_HOST_PCM;
    cfg.codec    = AURIS_CODEC_PCM_S16LE;
    cfg.vad_tier = AURIS_VAD_NEURAL;
    auris_engine_t* e = nullptr;
    const auris_status_t s = auris_create(&cfg, &e);
    if (s != AURIS_OK) return false;
    auris_destroy(e);
    return true;
}

} // namespace

TEST_CASE("Neural VAD: model loads when AURIS_NEURAL_VAD_MODEL points at the fixture") {
    setenv("AURIS_NEURAL_VAD_MODEL",
           (std::string(AURIS_TEST_FIXTURE_DIR) + "/silero_vad.onnx").c_str(),
           1);
    CHECK(neural_vad_available());
}

TEST_CASE("Neural VAD: detects speech in hello.wav, balanced events") {
    setenv("AURIS_NEURAL_VAD_MODEL",
           (std::string(AURIS_TEST_FIXTURE_DIR) + "/silero_vad.onnx").c_str(),
           1);
    const auto wav = read_wav_int16(
        std::string(AURIS_TEST_FIXTURE_DIR) + "/hello.wav");
    REQUIRE(wav.samples.size() > 0);
    REQUIRE(wav.sample_rate_hz == 16000);

    const auto neural = run_vad_through_fixture(wav, AURIS_VAD_NEURAL);

    INFO("NEURAL speech_begin=" << neural.speech_begin
         << " speech_end="     << neural.speech_end
         << " packets="        << neural.packets
         << " captured="       << neural.frames_captured
         << " dropped="        << neural.frames_dropped);

    CHECK(neural.speech_begin >= 1);
    // SPEECH_END from the final utterance can fire during auris_stop's
    // worker drain; allow at most 1 difference depending on event-poll
    // timing. Segment shape MUST balance because the worker emits
    // SEGMENT_END unconditionally on stop.
    CHECK(neural.speech_end >= neural.speech_begin - 1u);
    CHECK(neural.speech_end <= neural.speech_begin);
    CHECK(neural.segment_begin == neural.segment_end);
    CHECK(neural.frames_dropped == 0);
}

TEST_CASE("Neural VAD vs FAST VAD: differential on hello.wav, both detect speech") {
    setenv("AURIS_NEURAL_VAD_MODEL",
           (std::string(AURIS_TEST_FIXTURE_DIR) + "/silero_vad.onnx").c_str(),
           1);
    const auto wav = read_wav_int16(
        std::string(AURIS_TEST_FIXTURE_DIR) + "/hello.wav");
    REQUIRE(wav.samples.size() > 0);

    const auto fast   = run_vad_through_fixture(wav, AURIS_VAD_FAST);
    const auto neural = run_vad_through_fixture(wav, AURIS_VAD_NEURAL);

    INFO("FAST   begin/end=" << fast.speech_begin   << "/" << fast.speech_end
         << " packets="      << fast.packets);
    INFO("NEURAL begin/end=" << neural.speech_begin << "/" << neural.speech_end
         << " packets="      << neural.packets);

    CHECK(fast.speech_begin   >= 1);
    CHECK(neural.speech_begin >= 1);
    // Both detectors must produce at least one speech segment; their exact
    // counts may differ — Silero tends to merge close utterances that
    // libfvad splits, so the two need not match.
}

#else

TEST_CASE("Neural VAD tests skipped: AURIS_HAVE_NEURAL_VAD not defined in this build") {
    MESSAGE("ONNX Runtime not linked — skip");
    CHECK(true);
}

#endif  // AURIS_HAVE_NEURAL_VAD
