// tests/test_abi_fuzz.cpp
//
// ABI robustness / fuzz. The C surface must NEVER crash on hostile or
// malformed input: every call returns a status code, and on failure the
// out-params are left in a defined state. We throw thousands of random and
// structurally-invalid configs at mockingbird_create and assert the contract holds:
//   - no crash (implicit: a crash fails the run under ASan/UBSan)
//   - status != OK  =>  *out_engine == nullptr
//   - status == OK  =>  *out_engine != nullptr  (and destroy succeeds)
//   - mockingbird_status_str accepts any int and returns a non-null string
//   - null pointers are rejected with INVALID_ARG, never dereferenced

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
    uint64_t next() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return s;
    }
    uint32_t u32() { return static_cast<uint32_t>(next()); }
    int      i32() { return static_cast<int>(next()); }
    float    f()   { return static_cast<float>(static_cast<int32_t>(u32())) / 1024.0f; }
};

void random_config(mockingbird_config_t& c, Rng& r) {
    std::memset(&c, 0, sizeof(c));
    c.backend             = static_cast<mockingbird_backend_t>(r.u32() % 7);
    c.sample_rate_hz      = r.u32() % 5 == 0 ? r.u32() : (r.u32() % 64000);
    c.channels            = static_cast<uint16_t>(r.u32() % 4);
    c.sample_format       = static_cast<mockingbird_sample_format_t>(r.u32() % 3);
    c.frame_ms            = r.u32() % 50;
    c.enable_rnnoise      = r.i32();
    c.enable_vad          = r.i32();
    c.vad_tier            = static_cast<mockingbird_vad_tier_t>(r.u32() % 4);
    c.vad_aggressiveness  = r.i32() % 8 - 2;
    c.vad_neural_threshold= r.f();
    c.vad_hangover_ms     = r.u32() % 4000;
    c.enable_anomaly      = r.i32();
    c.anomaly_rms_z       = r.f();
    c.anomaly_flux_z      = r.f();
    c.anomaly_background_ms = r.u32() % 8000;
    c.pre_roll_ms         = r.u32() % 4000;
    c.post_roll_ms        = r.u32() % 4000;
    c.segment_max_ms      = r.u32();
    c.codec               = static_cast<mockingbird_codec_t>(r.u32() % 5);
    c.opus_bitrate_bps    = r.u32() % 600000;
    c.opus_vbr            = r.i32();
    c.opus_complexity     = r.i32() % 14 - 2;
    c.pcm_ring_frames     = r.u32() % 65536;
    c.packet_queue_capacity = r.u32() % 4096;
    c.enable_wait_handle  = r.i32();
    c.neural_vad_model_path = nullptr;
    c.enable_sound_class  = r.i32();
    c.sound_class_model_path = nullptr;
    c.sound_class_threshold  = r.f();
    c.sound_class_interval_ms = r.u32() % 4000;
}

}

TEST_CASE("ABI fuzz: null-pointer arguments are rejected, never dereferenced") {
    mockingbird_config_t cfg{};
    CHECK(mockingbird_config_defaults(nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_config_defaults(&cfg)    == MOCKINGBIRD_OK);

    mockingbird_engine_t* e = reinterpret_cast<mockingbird_engine_t*>(0xdeadbeef);
    CHECK(mockingbird_create(nullptr, &e) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(e == nullptr);
    CHECK(mockingbird_create(&cfg, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);

    CHECK(mockingbird_start(nullptr)  == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_stop(nullptr)   == MOCKINGBIRD_ERR_INVALID_ARG);
    mockingbird_destroy(nullptr);
    mockingbird_packet_t pkt{};
    CHECK(mockingbird_drain(nullptr, &pkt) == MOCKINGBIRD_ERR_INVALID_ARG);
    mockingbird_event_t evt{};
    CHECK(mockingbird_poll_event(nullptr, &evt) == MOCKINGBIRD_ERR_INVALID_ARG);
    mockingbird_stats_t st{};
    CHECK(mockingbird_get_stats(nullptr, &st) == MOCKINGBIRD_ERR_INVALID_ARG);
}

TEST_CASE("ABI fuzz: mockingbird_status_str accepts ANY int, returns a non-null string") {
    Rng r(0xA5A5A5A5u);
    for (int i = 0; i < 5000; ++i) {
        const char* s = mockingbird_status_str(r.i32());
        REQUIRE(s != nullptr);
        CHECK(std::strlen(s) > 0);
    }
    CHECK(std::string(mockingbird_status_str(MOCKINGBIRD_OK)) != "MOCKINGBIRD_UNKNOWN");
    CHECK(std::string(mockingbird_status_str(123456))   == "MOCKINGBIRD_UNKNOWN");
}

TEST_CASE("ABI fuzz: thousands of random configs never crash and honor the out-null contract") {
    Rng r(0xC0FFEEu);
    int created = 0, rejected = 0;
    for (int i = 0; i < 20000; ++i) {
        mockingbird_config_t cfg;
        random_config(cfg, r);

        mockingbird_engine_t* e = reinterpret_cast<mockingbird_engine_t*>(0x1);
        const mockingbird_status_t s = mockingbird_create(&cfg, &e);

        if (s == MOCKINGBIRD_OK) {
            REQUIRE(e != nullptr);
            mockingbird_destroy(e);
            ++created;
        } else {
            REQUIRE(e == nullptr);
            ++rejected;
        }
    }
    CHECK(rejected > 0);
    INFO("fuzz created=" << created << " rejected=" << rejected);
}

TEST_CASE("ABI fuzz: mockingbird_release rejects null / foreign / double-release safely") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend        = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec          = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.enable_vad     = 0;
    cfg.enable_anomaly = 0;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    CHECK(mockingbird_release(e, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);

    Rng r(0xBADF00Du);
    for (int i = 0; i < 2000; ++i) {
        mockingbird_packet_t pkt{};
        pkt._reserved = r.u32();
        pkt.data = reinterpret_cast<const uint8_t*>(
                       static_cast<uintptr_t>(r.next()));
        pkt.size = r.u32();
        CHECK(mockingbird_release(e, &pkt) == MOCKINGBIRD_ERR_INVALID_ARG);
    }

    std::vector<int16_t> frame(320, 1500);
    for (int k = 0; k < 4; ++k) {
        mockingbird_push_pcm(e, frame.data(), 320, static_cast<uint64_t>(k) * 20'000ull);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    mockingbird_packet_t pkt{};
    if (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) {
        CHECK(mockingbird_release(e, &pkt) == MOCKINGBIRD_OK);
        CHECK(mockingbird_release(e, &pkt) == MOCKINGBIRD_ERR_INVALID_ARG);
    }

    mockingbird_stop(e);
    mockingbird_destroy(e);
}

TEST_CASE("ABI fuzz: pathological queue sizes degrade to NO_MEMORY, never crash") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec   = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.pcm_ring_frames = 0xFFFFFFFFu;
    mockingbird_engine_t* e = nullptr;
    const mockingbird_status_t s = mockingbird_create(&cfg, &e);
    CHECK((s == MOCKINGBIRD_ERR_NO_MEMORY || s == MOCKINGBIRD_ERR_INVALID_ARG));
    CHECK(e == nullptr);
    if (s == MOCKINGBIRD_OK && e) mockingbird_destroy(e);
}
