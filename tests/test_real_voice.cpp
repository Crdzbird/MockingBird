// tests/test_real_voice.cpp
//
// Differential test against a real-voice fixture. The synthetic-burst tests
// in test_detectors_golden lock in shape invariants (event balance, pre-roll
// flag, anomaly trigger on stepped energy); this one feeds genuine human
// speech and asserts that the VAD actually fires.
//
// Fixture: tests/fixtures/hello.wav (16 kHz mono PCM_S16LE, generated on
// macOS with `say` + `afconvert`). Single utterance with leading + trailing
// silence so we expect: silent prefix, one or more SPEECH_BEGIN/END pairs
// over the speech, silent tail.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {
struct WavInts16 {
    std::vector<int16_t> samples;
    uint32_t             sample_rate_hz = 0;
    uint16_t             channels       = 0;
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

    uint16_t audio_format  = 0;
    uint16_t channels      = 0;
    uint32_t sample_rate   = 0;
    uint16_t bits_per_samp = 0;
    bool     fmt_seen      = false;

    while (true) {
        char     chunk_id[4];
        uint32_t chunk_size;
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
            if (!fmt_seen || audio_format != 1 || bits_per_samp != 16 || channels == 0) break;
            out.sample_rate_hz = sample_rate;
            out.channels       = channels;
            const size_t sample_count = chunk_size / sizeof(int16_t);
            out.samples.resize(sample_count);
            if (!read(out.samples.data(), chunk_size)) {
                out.samples.clear();
            }
            break;
        } else {
            if (chunk_size & 1u) ++chunk_size;
            if (std::fseek(f, static_cast<long>(chunk_size), SEEK_CUR) != 0) break;
        }
    }
    std::fclose(f);
    return out;
}

}

TEST_CASE("Differential: real speech triggers at least one SPEECH_BEGIN") {
    const std::string fixture =
        std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/hello.wav";
    const auto wav = read_wav_int16(fixture);
    REQUIRE(wav.samples.size() > 0);
    REQUIRE(wav.sample_rate_hz == 16000);
    REQUIRE(wav.channels       == 1);

    mockingbird_config_t cfg{};
    REQUIRE(mockingbird_config_defaults(&cfg) == MOCKINGBIRD_OK);
    cfg.backend            = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec              = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.sample_rate_hz     = 16000;
    cfg.frame_ms           = 20;
    cfg.enable_vad         = 1;
    cfg.enable_anomaly     = 0;
    cfg.vad_aggressiveness = 1;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    constexpr uint32_t kFrameSamples = 320;
    const size_t total_frames = wav.samples.size() / kFrameSamples;

    uint32_t speech_begin_count = 0;
    uint32_t speech_end_count   = 0;
    uint32_t segment_begin_cnt  = 0;
    uint32_t segment_end_cnt    = 0;
    uint32_t packets_drained    = 0;

    auto drain_events = [&]() {
        while (true) {
            mockingbird_event_t evt{};
            const mockingbird_status_t s = mockingbird_poll_event(e, &evt);
            if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) return;
            REQUIRE(s == MOCKINGBIRD_OK);
            if (evt.type == MOCKINGBIRD_EVT_SPEECH_BEGIN)  ++speech_begin_count;
            if (evt.type == MOCKINGBIRD_EVT_SPEECH_END)    ++speech_end_count;
            if (evt.type == MOCKINGBIRD_EVT_SEGMENT_BEGIN) ++segment_begin_cnt;
            if (evt.type == MOCKINGBIRD_EVT_SEGMENT_END)   ++segment_end_cnt;
        }
    };
    auto drain_packets = [&]() {
        while (true) {
            mockingbird_packet_t pkt{};
            const mockingbird_status_t s = mockingbird_drain(e, &pkt);
            if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) return;
            REQUIRE(s == MOCKINGBIRD_OK);
            ++packets_drained;
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
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    drain_events();
    drain_packets();

    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    drain_events();

    mockingbird_stats_t stats{};
    REQUIRE(mockingbird_get_stats(e, &stats) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);

    INFO("fixture frames=" << total_frames
                           << " speech_begin=" << speech_begin_count
                           << " speech_end=" << speech_end_count
                           << " packets=" << packets_drained
                           << " captured=" << stats.frames_captured);

    CHECK(speech_begin_count       >= 1);
    CHECK(speech_end_count         >= 1);
    CHECK(speech_begin_count       == speech_end_count);
    CHECK(segment_begin_cnt        == segment_end_cnt);
    CHECK(packets_drained          > 0);
    CHECK(stats.frames_dropped_capture == 0);
}
