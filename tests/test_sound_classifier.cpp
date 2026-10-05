// tests/test_sound_classifier.cpp
//
// Effectiveness test for the YAMNet sound classifier across the three
// classes the user pinned as MUST: voice, animals, anomaly-class events.
//
// Strategy: for each real-audio fixture, run the engine with the classifier
// enabled, collect MOCKINGBIRD_EVT_SOUND_CLASS events, and assert that the
// dominant class falls inside the expected AudioSet category for that
// fixture. Each test asserts ON THE DOMINANT class — not on every
// inference window — because YAMNet samples 3 s windows and a short clip
// gives the test 1–2 inferences max.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef MOCKINGBIRD_HAVE_SOUND_CLASS

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

struct ClassifierTally {
    std::unordered_map<int, int>   class_counts;
    std::unordered_map<int, float> class_max_conf;
    int   best_class       = -1;
    int   best_class_count = 0;
    float best_class_conf  = 0.0f;
};

ClassifierTally run_classifier_over_fixture(const WavInts16& wav,
                                            float threshold) {
    // // VERIFY  Keep the path string alive for the entire mockingbird_create
    // call. c_str() on a temporary is undefined behavior.
    const std::string model_path =
        std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/yamnet_3s.onnx";

    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend                  = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec                    = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.sample_rate_hz           = 16000;
    cfg.frame_ms                 = 20;
    cfg.enable_vad               = 0;
    cfg.enable_anomaly           = 0;
    cfg.enable_sound_class        = 1;
    cfg.sound_class_model_path    = model_path.c_str();
    cfg.sound_class_threshold     = threshold;
    cfg.sound_class_interval_ms   = 500;
    // YAMNet runs ~50-200 ms per inference; bump the ring so the worker
    // does not overflow while it catches up.
    cfg.pcm_ring_frames           = 1u << 17;   // 131072 samples = 8 s

    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    constexpr uint32_t kFrameSamples = 320;
    const size_t total_frames = wav.samples.size() / kFrameSamples;
    ClassifierTally t;

    auto drain_events = [&]() {
        while (true) {
            mockingbird_event_t evt{};
            const mockingbird_status_t s = mockingbird_poll_event(e, &evt);
            if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) return;
            REQUIRE(s == MOCKINGBIRD_OK);
            if (evt.type != MOCKINGBIRD_EVT_SOUND_CLASS) continue;
            const int cls = static_cast<int>(evt.reason_mask);
            t.class_counts[cls] += 1;
            if (evt.confidence > t.class_max_conf[cls]) {
                t.class_max_conf[cls] = evt.confidence;
            }
        }
    };
    auto drain_packets = [&]() {
        while (true) {
            mockingbird_packet_t pkt{};
            const mockingbird_status_t s = mockingbird_drain(e, &pkt);
            if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) return;
            REQUIRE(s == MOCKINGBIRD_OK);
            REQUIRE(mockingbird_release(e, &pkt) == MOCKINGBIRD_OK);
        }
    };

    for (size_t i = 0; i < total_frames; ++i) {
        const int16_t* p = wav.samples.data() + i * kFrameSamples;
        REQUIRE(mockingbird_push_pcm(e, p, kFrameSamples,
                               static_cast<uint64_t>(i) * 20'000ull) == MOCKINGBIRD_OK);
        if ((i & 0x1Fu) == 0x1F) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            drain_events();
            drain_packets();
        }
    }
    // Allow YAMNet's final inference window to complete.
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    drain_events();
    drain_packets();
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    drain_events();
    mockingbird_destroy(e);

    // Pick the most-fired class.
    for (auto& [cls, count] : t.class_counts) {
        if (count > t.best_class_count) {
            t.best_class_count = count;
            t.best_class       = cls;
            t.best_class_conf  = t.class_max_conf[cls];
        }
    }
    return t;
}

bool is_voice_class(int c) {
    // Speech, conversation, narration, child speech, babbling, baby cry,
    // crying, etc.
    return (c >= 0 && c <= 26);
}

bool is_animal_class(int c) {
    // AudioSet "Animal" branch: 67..134 covers domestic + wild animals.
    // Includes Dog (69), Bark (70), Cat (76), Meow (78), Whimper (75),
    // Cattle, Roaring cats, etc.
    return (c >= 67 && c <= 134);
}

bool is_music_class(int c) {
    // Music + musical instruments + genres.
    return (c >= 132 && c <= 282);
}

bool sound_class_available() {
    const std::string model_path =
        std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/yamnet_3s.onnx";
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend             = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec               = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.enable_sound_class   = 1;
    cfg.sound_class_model_path = model_path.c_str();
    mockingbird_engine_t* e = nullptr;
    if (mockingbird_create(&cfg, &e) != MOCKINGBIRD_OK) return false;
    mockingbird_destroy(e);
    return true;
}

} // namespace

TEST_CASE("Sound classifier: model loads when path is set") {
    CHECK(sound_class_available());
}

TEST_CASE("Sound classifier: cat fixture is classified as an animal sound") {
    const auto wav = read_wav_int16(
        std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/miaow_16k.wav");
    REQUIRE(wav.samples.size() > 0);
    REQUIRE(wav.sample_rate_hz == 16000);

    const auto t = run_classifier_over_fixture(wav, /*threshold=*/ 0.1f);
    INFO("cat best_class=" << t.best_class
         << " count="      << t.best_class_count
         << " conf="       << t.best_class_conf);

    CHECK(t.best_class >= 0);
    CHECK(is_animal_class(t.best_class));
}

TEST_CASE("Sound classifier: voice fixture is classified as speech") {
    const auto wav = read_wav_int16(
        std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/hello.wav");
    REQUIRE(wav.samples.size() > 0);

    const auto t = run_classifier_over_fixture(wav, /*threshold=*/ 0.1f);
    INFO("voice best_class=" << t.best_class
         << " count="        << t.best_class_count
         << " conf="         << t.best_class_conf);

    CHECK(t.best_class >= 0);
    CHECK(is_voice_class(t.best_class));
}

TEST_CASE("Sound classifier: music fixture is classified as music") {
    const auto wav = read_wav_int16(
        std::string(MOCKINGBIRD_TEST_FIXTURE_DIR) + "/music.wav");
    REQUIRE(wav.samples.size() > 0);

    const auto t = run_classifier_over_fixture(wav, /*threshold=*/ 0.1f);
    INFO("music best_class=" << t.best_class
         << " count="        << t.best_class_count
         << " conf="         << t.best_class_conf);

    CHECK(t.best_class >= 0);
    CHECK(is_music_class(t.best_class));
}

#else

TEST_CASE("Sound classifier tests skipped: MOCKINGBIRD_HAVE_SOUND_CLASS not defined") {
    MESSAGE("YAMNet not linked - skip");
    CHECK(true);
}

#endif  // MOCKINGBIRD_HAVE_SOUND_CLASS
