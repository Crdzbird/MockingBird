// Speaker verification through the C API, with two LibriSpeech speakers:
// speaker_a1.wav and speaker_a2.wav are one man, speaker_b.wav is a woman.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef MOCKINGBIRD_HAVE_SOUND_CLASS

namespace {
const std::string kFixtures = MOCKINGBIRD_TEST_FIXTURE_DIR;
const std::string kModel    = kFixtures + "/../../models/speaker_campplus.onnx";

std::vector<int16_t> pcm(const std::string& name) {
    std::vector<int16_t> out;
    std::FILE* f = std::fopen((kFixtures + "/" + name).c_str(), "rb");
    if (f == nullptr) return out;
    std::fseek(f, 44, SEEK_SET);
    int16_t s = 0;
    while (std::fread(&s, sizeof s, 1, f) == 1) out.push_back(s);
    std::fclose(f);
    return out;
}

mockingbird_engine_t* engine(uint32_t rate, const std::string& model) {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend            = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.sample_rate_hz     = rate;
    cfg.enable_anomaly     = 0;
    cfg.vad_aggressiveness = 3;
    cfg.pcm_ring_frames    = 1u << 20;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_set_speaker_model(e, model.c_str()) == MOCKINGBIRD_OK);
    return e;
}

std::vector<float> print(mockingbird_engine_t* e, const std::vector<int16_t>& s) {
    std::vector<float> v(256);
    uint32_t dim = 0;
    REQUIRE(mockingbird_speaker_embedding(e, s.data(), static_cast<uint32_t>(s.size()), v.data(), 256, &dim) ==
            MOCKINGBIRD_OK);
    v.resize(dim);
    return v;
}

float dot(const std::vector<float>& a, const std::vector<float>& b) {
    float d = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) d += a[i] * b[i];
    return d;
}

std::vector<mockingbird_event_t> verify(mockingbird_engine_t* e, const std::vector<int16_t>& audio) {
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    for (size_t i = 0; i + 320 <= audio.size(); i += 320) {
        REQUIRE(mockingbird_push_pcm(e, audio.data() + i, 320, i * 1'000'000ull / 16000) == MOCKINGBIRD_OK);
    }
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    std::vector<mockingbird_event_t> out;
    mockingbird_event_t ev{};
    while (mockingbird_poll_event(e, &ev) == MOCKINGBIRD_OK) {
        if (ev.type == MOCKINGBIRD_EVT_SPEAKER || ev.type == MOCKINGBIRD_EVT_BACKEND_ERROR) out.push_back(ev);
    }
    mockingbird_packet_t pkt{};
    while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) mockingbird_release(e, &pkt);
    return out;
}

}

TEST_CASE("Speaker: voice prints separate the two speakers") {
    mockingbird_engine_t* e = engine(16000, kModel);
    const auto a1 = print(e, pcm("speaker_a1.wav")), a2 = print(e, pcm("speaker_a2.wav"));
    const auto b  = print(e, pcm("speaker_b.wav"));
    REQUIRE(a1.size() == 192);
    CHECK(dot(a1, a1) == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(dot(a1, a2) > 0.6f);
    CHECK(dot(a1, b) < 0.3f);
    mockingbird_destroy(e);
}

TEST_CASE("Speaker: each utterance is reported as the target or not") {
    mockingbird_engine_t* e = engine(16000, kModel);
    const auto target = print(e, pcm("speaker_a1.wav"));
    REQUIRE(mockingbird_set_target_speaker(e, target.data(), static_cast<uint32_t>(target.size()), 0.0f) ==
            MOCKINGBIRD_OK);
    std::vector<int16_t> audio(16000, 0);
    for (const char* name : {"speaker_a2.wav", "speaker_b.wav"}) {
        const auto clip = pcm(name);
        audio.insert(audio.end(), clip.begin(), clip.end());
        audio.insert(audio.end(), 32000, 0);
    }
    const auto events = verify(e, audio);
    REQUIRE(events.size() >= 2);
    const uint64_t boundary = (16000ull + 80000ull + 16000ull) * 1'000'000ull / 16000;
    for (const auto& ev : events) {
        REQUIRE(ev.type == MOCKINGBIRD_EVT_SPEAKER);
        CHECK(ev.reason_mask == (ev.timestamp_us < boundary ? 1u : 0u));
    }
    CHECK(events.front().confidence > 0.5f);
    CHECK(events.back().confidence < 0.5f);
    REQUIRE(mockingbird_set_target_speaker(e, nullptr, 0, 0.0f) == MOCKINGBIRD_OK);
    CHECK(verify(e, audio).empty());
    mockingbird_destroy(e);
}

TEST_CASE("Speaker: arguments, sample rate and a missing model are reported") {
    mockingbird_engine_t* e = engine(16000, kFixtures + "/missing.onnx");
    const std::vector<float> fake(192, 0.1f);
    std::vector<int16_t> brief(4000, 100);
    float out[4];
    uint32_t dim = 0;
    CHECK(mockingbird_speaker_embedding(e, brief.data(), 4000, out, 4, &dim) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_set_target_speaker(e, fake.data(), 0, 0.5f) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_set_target_speaker(e, fake.data(), 192, 1.5f) == MOCKINGBIRD_ERR_INVALID_ARG);
    REQUIRE(mockingbird_set_target_speaker(e, fake.data(), 192, 0.5f) == MOCKINGBIRD_OK);
    const auto events = verify(e, pcm("speaker_a2.wav"));
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == MOCKINGBIRD_EVT_BACKEND_ERROR);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    CHECK(mockingbird_set_speaker_model(e, kModel.c_str()) == MOCKINGBIRD_ERR_INVALID_STATE);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
    mockingbird_engine_t* low = engine(8000, kModel);
    CHECK(mockingbird_set_target_speaker(low, fake.data(), 192, 0.5f) == MOCKINGBIRD_ERR_UNSUPPORTED);
    mockingbird_destroy(low);
}

#endif
