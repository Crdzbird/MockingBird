// tests/test_preprocess_rnnoise.cpp
//
// Covers the RNNoise preprocessing front-end and its 48 kHz resampling bridge:
//   1. The polyphase resampler round-trips a tone with energy preserved and no
//      blow-up (header-only, tested directly).
//   2. RNNoise measurably suppresses stationary noise.
//   3. RNNoise preserves speech (the hello.wav fixture keeps its energy).
//   4. validate_config accepts integer-ratio rates and rejects 32 kHz.
//   5. The engine runs end-to-end with enable_rnnoise and still detects speech.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "preprocess/resampler.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {
double rms_i16(const int16_t* p, size_t n) {
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) s += double(p[i]) * double(p[i]);
    return std::sqrt(s / double(n));
}

double rms_f(const std::vector<float>& v, size_t from, size_t to) {
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += double(v[i]) * double(v[i]);
    return std::sqrt(s / double(to - from));
}

struct Lcg {
    uint32_t st = 0x12345678u;
    float next() {
        st = st * 1664525u + 1013904223u;
        return (float(st >> 8) / float(1u << 24)) * 2.0f - 1.0f;
    }
};

}

TEST_CASE("Resampler: x3 up/down round-trip preserves a 1 kHz tone's energy") {
    mockingbird::Interpolator up;
    mockingbird::Decimator    down;
    up.configure(3);
    down.configure(3);

    std::vector<float> in(4000);
    for (size_t i = 0; i < in.size(); ++i) {
        in[i] = static_cast<float>(8000.0 * std::sin(2.0 * 3.14159265358979 *
                                   1000.0 * double(i) / 16000.0));
    }
    std::vector<float> up_buf, back;
    up.process(in.data(), in.size(), up_buf);
    down.process(up_buf.data(), up_buf.size(), back);

    REQUIRE(up_buf.size() == in.size() * 3);
    REQUIRE(back.size()  == in.size());

    const double e_in   = rms_f(in,   500, in.size()   - 500);
    const double e_back = rms_f(back, 500, back.size() - 500);
    CHECK(e_back > 0.7 * e_in);
    CHECK(e_back < 1.3 * e_in);
}

TEST_CASE("Resampler: an out-of-band 7 kHz tone survives the 8 kHz passband") {
    mockingbird::Interpolator up; mockingbird::Decimator down;
    up.configure(3); down.configure(3);
    std::vector<float> in(4000);
    for (size_t i = 0; i < in.size(); ++i)
        in[i] = static_cast<float>(8000.0 * std::sin(2.0 * 3.14159265358979 *
                                   7000.0 * double(i) / 16000.0));
    std::vector<float> up_buf, back;
    up.process(in.data(), in.size(), up_buf);
    down.process(up_buf.data(), up_buf.size(), back);
    const double e_in   = rms_f(in,   600, in.size()   - 600);
    const double e_back = rms_f(back, 600, back.size() - 600);
    CHECK(e_back > 0.4 * e_in);
}

#ifdef MOCKINGBIRD_HAVE_RNNOISE

#include "preprocess/preprocessor.hpp"

TEST_CASE("RNNoise: suppresses stationary noise") {
    auto pp = mockingbird::make_rnnoise_preprocessor();
    REQUIRE(pp != nullptr);
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.sample_rate_hz = 16000;
    cfg.frame_ms       = 20;
    REQUIRE(pp->init(cfg) == MOCKINGBIRD_OK);

    Lcg rng;
    constexpr uint32_t N = 320;
    double in_rms_tail = 0.0, out_rms_tail = 0.0;
    int tail = 0;
    for (int f = 0; f < 120; ++f) {
        int16_t frame[N];
        for (uint32_t i = 0; i < N; ++i)
            frame[i] = int16_t(5000.0f * rng.next());
        const double in_rms = rms_i16(frame, N);
        pp->process(frame, N);
        const double out_rms = rms_i16(frame, N);
        if (f >= 90) {
            in_rms_tail  += in_rms;
            out_rms_tail += out_rms;
            ++tail;
        }
    }
    in_rms_tail  /= tail;
    out_rms_tail /= tail;
    INFO("noise in_rms=" << in_rms_tail << " out_rms=" << out_rms_tail);
    CHECK(out_rms_tail < 0.8 * in_rms_tail);
}

namespace {
struct Wav { std::vector<int16_t> s; uint32_t sr = 0; };
Wav read_wav(const std::string& path) {
    Wav out; std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return out;
    auto rd = [&](void* d, size_t n){ return std::fread(d, 1, n, f) == n; };
    char riff[4]; uint32_t sz; char wave[4];
    if (!rd(riff,4) || std::memcmp(riff,"RIFF",4) || !rd(&sz,4) ||
        !rd(wave,4) || std::memcmp(wave,"WAVE",4)) { std::fclose(f); return out; }
    uint16_t fmt=0, ch=0, bps=0; uint32_t sr=0; bool seen=false;
    while (true) {
        char id[4]; uint32_t cs;
        if (!rd(id,4) || !rd(&cs,4)) break;
        if (!std::memcmp(id,"fmt ",4)) {
            std::vector<uint8_t> b(cs); if (!rd(b.data(),cs)) break;
            fmt=uint16_t(b[0]|b[1]<<8); ch=uint16_t(b[2]|b[3]<<8);
            sr=uint32_t(b[4]|b[5]<<8|b[6]<<16|b[7]<<24); bps=uint16_t(b[14]|b[15]<<8);
            seen=true;
        } else if (!std::memcmp(id,"data",4)) {
            if (!seen||fmt!=1||bps!=16||ch!=1) break;
            out.sr=sr; out.s.resize(cs/2);
            if (!rd(out.s.data(),cs)) out.s.clear();
            break;
        } else { if (cs&1) ++cs; if (std::fseek(f,long(cs),SEEK_CUR)) break; }
    }
    std::fclose(f); return out;
}
}

TEST_CASE("RNNoise: preserves speech energy (hello.wav)") {
    const auto wav = read_wav(std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/hello.wav");
    REQUIRE(wav.s.size() > 0);
    REQUIRE(wav.sr == 16000);

    auto pp = mockingbird::make_rnnoise_preprocessor();
    REQUIRE(pp != nullptr);
    mockingbird_config_t cfg{}; mockingbird_config_defaults(&cfg);
    cfg.sample_rate_hz = 16000; cfg.frame_ms = 20;
    REQUIRE(pp->init(cfg) == MOCKINGBIRD_OK);

    constexpr uint32_t N = 320;
    const size_t frames = wav.s.size() / N;
    double in_energy = 0.0, out_energy = 0.0;
    for (size_t fr = 0; fr < frames; ++fr) {
        int16_t buf[N];
        std::memcpy(buf, wav.s.data() + fr * N, N * sizeof(int16_t));
        in_energy += rms_i16(buf, N);
        pp->process(buf, N);
        out_energy += rms_i16(buf, N);
    }
    INFO("speech in=" << in_energy << " out=" << out_energy);
    CHECK(out_energy > 0.3 * in_energy);
}

TEST_CASE("Config: enable_rnnoise accepts 16 kHz, rejects 32 kHz") {
    mockingbird_config_t cfg{}; mockingbird_config_defaults(&cfg);
    cfg.backend = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec   = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.enable_rnnoise = 1;

    cfg.sample_rate_hz = 16000;
    mockingbird_engine_t* e = nullptr;
    CHECK(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    if (e) mockingbird_destroy(e);

    cfg.sample_rate_hz = 32000;
    e = nullptr;
    CHECK(mockingbird_create(&cfg, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(e == nullptr);
}

TEST_CASE("Engine: runs end-to-end with RNNoise and still detects speech") {
    const auto wav = read_wav(std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/hello.wav");
    REQUIRE(wav.s.size() > 0);

    mockingbird_config_t cfg{}; mockingbird_config_defaults(&cfg);
    cfg.backend            = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec              = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.sample_rate_hz     = 16000;
    cfg.frame_ms           = 20;
    cfg.enable_rnnoise     = 1;
    cfg.enable_vad         = 1;
    cfg.enable_anomaly     = 0;
    cfg.vad_aggressiveness = 1;
    cfg.pcm_ring_frames    = 1u << 15;

    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    constexpr uint32_t N = 320;
    const size_t frames = wav.s.size() / N;
    uint32_t speech_begin = 0;
    auto drain = [&]{
        mockingbird_event_t ev{};
        while (mockingbird_poll_event(e, &ev) == MOCKINGBIRD_OK)
            if (ev.type == MOCKINGBIRD_EVT_SPEECH_BEGIN) ++speech_begin;
    };
    for (size_t i = 0; i < frames; ++i) {
        REQUIRE(mockingbird_push_pcm(e, wav.s.data() + i * N, N,
                               uint64_t(i) * 20'000ull) == MOCKINGBIRD_OK);
        if ((i & 0x1Fu) == 0x1F) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            drain();
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    drain();
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    drain();

    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);

    INFO("rnnoise+vad speech_begin=" << speech_begin
         << " captured=" << st.frames_captured
         << " dropped=" << st.frames_dropped_capture);
    CHECK(speech_begin >= 1);
}

#else
TEST_CASE("RNNoise tests skipped: MOCKINGBIRD_HAVE_RNNOISE not defined") {
    MESSAGE("RNNoise not compiled in, skip");
    CHECK(true);
}
#endif
