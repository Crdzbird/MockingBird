// bench/throughput.cpp
//
// Throughput benchmark for the Auris engine: how much CPU does one second of
// audio cost under each feature combination?
//
// Method: push N seconds of deterministic synthetic audio through HOST_PCM as
// fast as the ring accepts it. The worker thread is then the bottleneck, so
// the wall time until stats.frames_captured reaches the total IS the worker's
// cost. We cross-check with process CPU time (the pusher mostly sleeps).
//
//   ./bench_auris [seconds]        (default 30)
//
// Output per config: real-time factor (CPU seconds per audio second) and the
// equivalent share of one core at real time. RTF 0.01 == 1% of a core.

#include <auris/auris.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <sys/resource.h>

namespace {

constexpr uint32_t kRate  = 16000;
constexpr uint32_t kFrame = 320;   // 20 ms

// Integer-only xorshift so every run (and every platform) sees identical audio.
struct Rng {
    uint64_t s = 0x9E3779B97F4A7C15ull;
    uint32_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return static_cast<uint32_t>(s >> 32); }
};

// Realistic-enough content: pink-ish noise floor with 300 ms "voice-like"
// bursts every 1.5 s so VAD / anomaly / YAMNet all have transitions to chase.
std::vector<int16_t> make_audio(uint32_t seconds) {
    std::vector<int16_t> out(static_cast<size_t>(seconds) * kRate);
    Rng rng; float lp = 0.f;
    for (size_t i = 0; i < out.size(); ++i) {
        const float white = (static_cast<float>(rng.next() & 0xFFFF) / 32768.f) - 1.f;
        lp = 0.97f * lp + 0.03f * white;                      // pink-ish floor
        float v = lp * 1200.f;
        const size_t t = i % (kRate * 3 / 2);
        if (t < kRate * 3 / 10) {                              // 300 ms burst
            const float ph = static_cast<float>(i) * (2.f * 3.14159265f * 220.f / kRate);
            v += 9000.f * std::sin(ph) * (0.6f + 0.4f * std::sin(ph * 0.013f));
        }
        out[i] = static_cast<int16_t>(std::fmax(-32768.f, std::fmin(32767.f, v)));
    }
    return out;
}

double cpu_seconds() {
    rusage ru{}; getrusage(RUSAGE_SELF, &ru);
    return ru.ru_utime.tv_sec + ru.ru_utime.tv_usec * 1e-6 +
           ru.ru_stime.tv_sec + ru.ru_stime.tv_usec * 1e-6;
}

struct Case { const char* name; auris_config_t cfg; };

auris_config_t base() {
    auris_config_t c{}; auris_config_defaults(&c);
    c.backend = AURIS_BACKEND_HOST_PCM; c.sample_rate_hz = kRate; c.frame_ms = 20;
    c.codec = AURIS_CODEC_PCM_S16LE;
    c.enable_vad = 0; c.enable_anomaly = 0; c.enable_rnnoise = 0; c.enable_sound_class = 0;
    c.pcm_ring_frames = 1u << 17; c.packet_queue_capacity = 4096;
    return c;
}

void run(const Case& k, const std::vector<int16_t>& audio) {
    // AURIS_BENCH_ONLY=<substring> runs just the matching cases.
    if (const char* only = std::getenv("AURIS_BENCH_ONLY"); only && !std::strstr(k.name, only)) return;
    auris_engine_t* e = nullptr;
    auris_status_t st = auris_create(&k.cfg, &e);
    if (st != AURIS_OK) { std::printf("%-28s  SKIPPED (%s)\n", k.name, auris_status_str(st)); return; }
    auris_start(e);

    const auto   t0   = std::chrono::steady_clock::now();
    const double cpu0 = cpu_seconds();
    const size_t frames = audio.size() / kFrame;
    auris_packet_t pkt{}; auris_event_t evt{};
    // A full ring DROPS (push returns OK and bumps frames_dropped_capture), so
    // pace on the ring fill instead: keep it at least one frame short of full.
    // That keeps the worker saturated without ever losing audio.
    const uint32_t ring_cap = k.cfg.pcm_ring_frames;
    for (size_t f = 0; f < frames; ++f) {
        for (;;) {
            auris_stats_t s{}; auris_get_stats(e, &s);
            if (s.pcm_ring_fill_frames + 2 * kFrame <= ring_cap) break;
            while (auris_drain(e, &pkt) == AURIS_OK) auris_release(e, &pkt);
            while (auris_poll_event(e, &evt) == AURIS_OK) {}
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        auris_push_pcm(e, audio.data() + f * kFrame, kFrame, static_cast<uint64_t>(f) * 20000);
    }
    auris_stats_t s{};
    for (;;) {
        auris_get_stats(e, &s);
        if (s.frames_captured + s.frames_dropped_capture >= frames * kFrame &&
            s.pcm_ring_fill_frames < kFrame) break;
        while (auris_drain(e, &pkt) == AURIS_OK) auris_release(e, &pkt);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double cpu  = cpu_seconds() - cpu0;
    auris_stop(e); auris_destroy(e);
    if (s.frames_dropped_capture != 0) {
        std::printf("%-28s  INVALID: %llu samples dropped at capture\n", k.name,
                    static_cast<unsigned long long>(s.frames_dropped_capture));
        return;
    }

    const double audio_s = static_cast<double>(frames * kFrame) / kRate;
    std::printf("%-28s  RTF %.4f  (%5.2f%% of a core)   cpu/wall %.2f   segs %llu  pkts %llu\n",
                k.name, wall / audio_s, 100.0 * wall / audio_s, cpu / wall,
                static_cast<unsigned long long>(s.segments_opened),
                static_cast<unsigned long long>(s.packets_emitted));
}

} // namespace

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    const uint32_t seconds = argc > 1 ? static_cast<uint32_t>(std::atoi(argv[1])) : 30;
    const std::string fx = AURIS_BENCH_FIXTURE_DIR;
    const std::string silero = fx + "/silero_vad.onnx", yamnet = fx + "/yamnet_3s.onnx";
    const auto audio = make_audio(seconds);
    // Same length, but a -75 dBFS room floor: everything sits under the silence
    // gate, so RNNoise and YAMNet should shed almost all of their work.
    std::vector<int16_t> quiet(audio.size());
    { Rng rng; for (auto& v : quiet) v = static_cast<int16_t>(static_cast<int>(rng.next() % 13) - 6); }
    std::printf("Auris %s — %u s of synthetic 16 kHz audio per config\n\n", auris_version_string(), seconds);

    std::vector<Case> cases;
    { auto c = base(); cases.push_back({"passthrough (PCM)", c}); }
    { auto c = base(); c.enable_vad = 1; c.vad_tier = AURIS_VAD_FAST; cases.push_back({"fast VAD", c}); }
    { auto c = base(); c.enable_anomaly = 1; cases.push_back({"anomaly", c}); }
    { auto c = base(); c.enable_vad = 1; c.vad_tier = AURIS_VAD_FAST; c.enable_anomaly = 1; cases.push_back({"fast VAD + anomaly (default)", c}); }
    { auto c = base(); c.codec = AURIS_CODEC_OPUS; cases.push_back({"Opus encode", c}); }
    { auto c = base(); c.enable_rnnoise = 1; cases.push_back({"RNNoise", c}); }
    { auto c = base(); c.enable_vad = 1; c.vad_tier = AURIS_VAD_NEURAL; c.neural_vad_model_path = silero.c_str(); cases.push_back({"neural VAD (Silero)", c}); }
    { auto c = base(); c.enable_sound_class = 1; c.sound_class_model_path = yamnet.c_str(); c.sound_class_interval_ms = 500; cases.push_back({"YAMNet @500ms (old default)", c}); }
    { auto c = base(); c.enable_sound_class = 1; c.sound_class_model_path = yamnet.c_str(); c.sound_class_interval_ms = 1000; cases.push_back({"YAMNet @1000ms (new default)", c}); }
    { auto c = base(); c.enable_vad = 1; c.vad_tier = AURIS_VAD_NEURAL; c.neural_vad_model_path = silero.c_str();
      c.enable_anomaly = 1; c.enable_rnnoise = 1; c.enable_sound_class = 1; c.sound_class_model_path = yamnet.c_str();
      c.codec = AURIS_CODEC_OPUS; cases.push_back({"EVERYTHING (+Opus)", c}); }
    for (const auto& k : cases) run(k, audio);

    std::printf("\n-- silence gate: identical configs, -75 dBFS room tone --\n");
    std::vector<Case> quiet_cases;
    { auto c = base(); c.enable_rnnoise = 1; quiet_cases.push_back({"RNNoise, quiet room", c}); }
    { auto c = base(); c.enable_sound_class = 1; c.sound_class_model_path = yamnet.c_str(); quiet_cases.push_back({"YAMNet @1000ms, quiet room", c}); }
    { auto c = base(); c.enable_vad = 1; c.vad_tier = AURIS_VAD_NEURAL; c.neural_vad_model_path = silero.c_str();
      c.enable_anomaly = 1; c.enable_rnnoise = 1; c.enable_sound_class = 1; c.sound_class_model_path = yamnet.c_str();
      c.codec = AURIS_CODEC_OPUS; quiet_cases.push_back({"EVERYTHING, quiet room", c}); }
    for (const auto& k : quiet_cases) run(k, quiet);
    return 0;
}
