// Keyword spotting through the C API with tests/fixtures/keyword_test.onnx,
// a classifier built by scripts/make_keyword_test_model.py that scores the
// speech in hello.wav. Its scores there cross 0.5 at the 16th 80 ms step and
// stay up for the rest period, and cross 0.9 once, at the 27th.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef MOCKINGBIRD_HAVE_SOUND_CLASS

namespace {
const std::string kFixtures = MOCKINGBIRD_TEST_FIXTURE_DIR;
const std::string kAssets   = kFixtures + "/../../models";

std::vector<int16_t> read_pcm(const std::string& path) {
    std::vector<int16_t> out;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return out;
    std::vector<char> bytes;
    char buf[4096];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) bytes.insert(bytes.end(), buf, buf + n);
    std::fclose(f);
    for (size_t i = 12; i + 8 <= bytes.size();) {
        uint32_t size = 0;
        std::memcpy(&size, &bytes[i + 4], 4);
        if (std::memcmp(&bytes[i], "data", 4) == 0) {
            out.resize(size / 2);
            std::memcpy(out.data(), &bytes[i + 8], out.size() * 2);
            return out;
        }
        i += 8 + size + (size & 1u);
    }
    return out;
}

mockingbird_engine_t* make_engine(uint32_t rate) {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend         = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.sample_rate_hz  = rate;
    cfg.pcm_ring_frames = 1u << 18;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_set_keyword_frontend(e, (kAssets + "/melspectrogram.onnx").c_str(),
                                             (kAssets + "/embedding_model.onnx").c_str()) == MOCKINGBIRD_OK);
    return e;
}

std::vector<mockingbird_event_t> run(mockingbird_engine_t* e, const std::vector<int16_t>& pcm, uint32_t frame) {
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    for (size_t i = 0; i + frame <= pcm.size(); i += frame) {
        REQUIRE(mockingbird_push_pcm(e, pcm.data() + i, frame, i * 1'000'000ull / 16000) == MOCKINGBIRD_OK);
    }
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    std::vector<mockingbird_event_t> out;
    mockingbird_event_t ev{};
    while (mockingbird_poll_event(e, &ev) == MOCKINGBIRD_OK) {
        if (ev.type == MOCKINGBIRD_EVT_KEYWORD || ev.type == MOCKINGBIRD_EVT_BACKEND_ERROR) out.push_back(ev);
    }
    mockingbird_packet_t pkt{};
    while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) mockingbird_release(e, &pkt);
    return out;
}

}

TEST_CASE("Keywords: fire on the expected 80 ms steps, with the rest period, for each threshold") {
    const auto pcm = read_pcm(kFixtures + "/hello.wav");
    REQUIRE(pcm.size() > 16000u);
    mockingbird_engine_t* e = make_engine(16000);
    uint32_t low = 99, high = 99;
    REQUIRE(mockingbird_add_keyword(e, (kFixtures + "/keyword_test.onnx").c_str(), 0.5f, &low) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_add_keyword(e, (kFixtures + "/keyword_test.onnx").c_str(), 0.9f, &high) == MOCKINGBIRD_OK);
    CHECK(low == 0);
    CHECK(high == 1);

    for (uint32_t frame : {320u, 480u}) {
        const auto events = run(e, pcm, frame);
        REQUIRE(events.size() == 3);
        const uint32_t ids[]   = {0, 1, 0};
        const uint64_t steps[] = {16, 27, 28};
        for (size_t k = 0; k < 3; ++k) {
            CHECK(events[k].type == MOCKINGBIRD_EVT_KEYWORD);
            CHECK(events[k].reason_mask == ids[k]);
            const uint64_t end_us = steps[k] * 80'000ull;
            CHECK(events[k].timestamp_us < end_us);
            CHECK(events[k].timestamp_us + frame * 1'000'000ull / 16000 >= end_us);
            CHECK(events[k].confidence >= (ids[k] == 0 ? 0.5f : 0.9f));
        }
    }
    mockingbird_destroy(e);
}

TEST_CASE("Keywords: the C API checks its arguments and the engine state") {
    mockingbird_engine_t* e = make_engine(16000);
    const std::string model = kFixtures + "/keyword_test.onnx";
    CHECK(mockingbird_add_keyword(nullptr, model.c_str(), 0.5f, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_add_keyword(e, nullptr, 0.5f, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_add_keyword(e, "", 0.5f, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_add_keyword(e, model.c_str(), 1.0f, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_add_keyword(e, model.c_str(), -0.1f, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    for (int i = 0; i < 16; ++i) CHECK(mockingbird_add_keyword(e, model.c_str(), 0.0f, nullptr) == MOCKINGBIRD_OK);
    CHECK(mockingbird_add_keyword(e, model.c_str(), 0.0f, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_clear_keywords(e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_add_keyword(e, model.c_str(), 0.0f, nullptr) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    CHECK(mockingbird_add_keyword(e, model.c_str(), 0.5f, nullptr) == MOCKINGBIRD_ERR_INVALID_STATE);
    CHECK(mockingbird_clear_keywords(e) == MOCKINGBIRD_ERR_INVALID_STATE);
    CHECK(mockingbird_set_keyword_frontend(e, nullptr, nullptr) == MOCKINGBIRD_ERR_INVALID_STATE);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}

TEST_CASE("Keywords: a missing model or another sample rate is a backend error, not a crash") {
    const auto pcm = read_pcm(kFixtures + "/hello.wav");
    for (uint32_t rate : {16000u, 8000u}) {
        mockingbird_engine_t* e = make_engine(rate);
        const std::string model = rate == 16000 ? kFixtures + "/missing.onnx" : kFixtures + "/keyword_test.onnx";
        REQUIRE(mockingbird_add_keyword(e, model.c_str(), 0.5f, nullptr) == MOCKINGBIRD_OK);
        const auto events = run(e, pcm, rate / 50);
        REQUIRE(events.size() == 1);
        CHECK(events[0].type == MOCKINGBIRD_EVT_BACKEND_ERROR);
        CHECK(static_cast<int32_t>(events[0].reason_mask) == MOCKINGBIRD_ERR_UNSUPPORTED);
        mockingbird_destroy(e);
    }
}

#endif
