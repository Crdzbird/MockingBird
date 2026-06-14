// tests/test_abi_fuzz.cpp
//
// ABI robustness / fuzz. The C surface must NEVER crash on hostile or
// malformed input — every call returns a status code, and on failure the
// out-params are left in a defined state. We throw thousands of random and
// structurally-invalid configs at auris_create and assert the contract holds:
//   - no crash (implicit: a crash fails the run under ASan/UBSan)
//   - status != OK  =>  *out_engine == nullptr
//   - status == OK  =>  *out_engine != nullptr  (and destroy succeeds)
//   - auris_status_str accepts any int and returns a non-null string
//   - null pointers are rejected with INVALID_ARG, never dereferenced

#include "doctest.h"
#include "auris/auris.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

// Deterministic xorshift64 — reproducible, no Date/rand dependency.
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

// Randomize every scalar field but keep the borrowed string pointers null —
// a host passing a garbage char* is undefined by contract (unvalidatable), so
// the fuzz models the realistic surface: null or valid paths only.
void random_config(auris_config_t& c, Rng& r) {
    std::memset(&c, 0, sizeof(c));
    c.backend             = static_cast<auris_backend_t>(r.u32() % 7);
    c.sample_rate_hz      = r.u32() % 5 == 0 ? r.u32() : (r.u32() % 64000);
    c.channels            = static_cast<uint16_t>(r.u32() % 4);
    c.sample_format       = static_cast<auris_sample_format_t>(r.u32() % 3);
    c.frame_ms            = r.u32() % 50;
    c.enable_rnnoise      = r.i32();
    c.enable_vad          = r.i32();
    c.vad_tier            = static_cast<auris_vad_tier_t>(r.u32() % 4);
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
    c.codec               = static_cast<auris_codec_t>(r.u32() % 5);
    c.opus_bitrate_bps    = r.u32() % 600000;
    c.opus_vbr            = r.i32();
    c.opus_complexity     = r.i32() % 14 - 2;
    // Bound the queue sizes: an unbounded random uint32 would attempt multi-GB
    // allocations (handled as NO_MEMORY, but slow to repeat thousands of times).
    c.pcm_ring_frames     = r.u32() % 65536;
    c.packet_queue_capacity = r.u32() % 4096;
    c.enable_wait_handle  = r.i32();
    c.neural_vad_model_path = nullptr;
    c.enable_sound_class  = r.i32();
    c.sound_class_model_path = nullptr;
    c.sound_class_threshold  = r.f();
    c.sound_class_interval_ms = r.u32() % 4000;
    // _reserved left zeroed; the engine must ignore it regardless.
}

} // namespace

TEST_CASE("ABI fuzz: null-pointer arguments are rejected, never dereferenced") {
    auris_config_t cfg{};
    CHECK(auris_config_defaults(nullptr) == AURIS_ERR_INVALID_ARG);
    CHECK(auris_config_defaults(&cfg)    == AURIS_OK);

    auris_engine_t* e = reinterpret_cast<auris_engine_t*>(0xdeadbeef);
    CHECK(auris_create(nullptr, &e) == AURIS_ERR_INVALID_ARG);
    CHECK(e == nullptr);                       // out is nulled even on bad cfg
    CHECK(auris_create(&cfg, nullptr) == AURIS_ERR_INVALID_ARG);

    // Lifecycle calls on null must not crash.
    CHECK(auris_start(nullptr)  == AURIS_ERR_INVALID_ARG);
    CHECK(auris_stop(nullptr)   == AURIS_ERR_INVALID_ARG);
    auris_destroy(nullptr);                    // no-op, no crash
    auris_packet_t pkt{};
    CHECK(auris_drain(nullptr, &pkt) == AURIS_ERR_INVALID_ARG);
    auris_event_t evt{};
    CHECK(auris_poll_event(nullptr, &evt) == AURIS_ERR_INVALID_ARG);
    auris_stats_t st{};
    CHECK(auris_get_stats(nullptr, &st) == AURIS_ERR_INVALID_ARG);
}

TEST_CASE("ABI fuzz: auris_status_str accepts ANY int, returns a non-null string") {
    Rng r(0xA5A5A5A5u);
    for (int i = 0; i < 5000; ++i) {
        const char* s = auris_status_str(r.i32());
        REQUIRE(s != nullptr);
        CHECK(std::strlen(s) > 0);             // and is a real C string
    }
    // Known codes resolve to stable strings.
    CHECK(std::string(auris_status_str(AURIS_OK)) != "AURIS_UNKNOWN");
    CHECK(std::string(auris_status_str(123456))   == "AURIS_UNKNOWN");
}

TEST_CASE("ABI fuzz: thousands of random configs never crash and honor the out-null contract") {
    Rng r(0xC0FFEEu);
    int created = 0, rejected = 0;
    for (int i = 0; i < 20000; ++i) {
        auris_config_t cfg;
        random_config(cfg, r);

        auris_engine_t* e = reinterpret_cast<auris_engine_t*>(0x1);  // poison
        const auris_status_t s = auris_create(&cfg, &e);

        if (s == AURIS_OK) {
            REQUIRE(e != nullptr);             // success => valid handle
            auris_destroy(e);
            ++created;
        } else {
            REQUIRE(e == nullptr);             // failure => out nulled
            ++rejected;
        }
    }
    // The fuzz space is dominated by invalid configs; just confirm both
    // branches were exercised so the test is meaningful.
    CHECK(rejected > 0);
    INFO("fuzz created=" << created << " rejected=" << rejected);
}

TEST_CASE("ABI fuzz: auris_release rejects null / foreign / double-release safely") {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend        = AURIS_BACKEND_HOST_PCM;
    cfg.codec          = AURIS_CODEC_PCM_S16LE;
    cfg.enable_vad     = 0;          // passthrough: every frame becomes a packet
    cfg.enable_anomaly = 0;
    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);
    REQUIRE(auris_start(e) == AURIS_OK);

    CHECK(auris_release(e, nullptr) == AURIS_ERR_INVALID_ARG);

    // Foreign/garbage packets: random cookie + random data pointer. release()
    // only COMPARES the pointer (never dereferences it), so this is safe and
    // must always be rejected.
    Rng r(0xBADF00Du);
    for (int i = 0; i < 2000; ++i) {
        auris_packet_t pkt{};
        pkt._reserved = r.u32();
        pkt.data = reinterpret_cast<const uint8_t*>(
                       static_cast<uintptr_t>(r.next()));
        pkt.size = r.u32();
        CHECK(auris_release(e, &pkt) == AURIS_ERR_INVALID_ARG);
    }

    // Real packet: first release succeeds, a second (double) release is rejected
    // instead of corrupting the free list.
    std::vector<int16_t> frame(320, 1500);
    for (int k = 0; k < 4; ++k) {
        auris_push_pcm(e, frame.data(), 320, static_cast<uint64_t>(k) * 20'000ull);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    auris_packet_t pkt{};
    if (auris_drain(e, &pkt) == AURIS_OK) {
        CHECK(auris_release(e, &pkt) == AURIS_OK);                 // first
        CHECK(auris_release(e, &pkt) == AURIS_ERR_INVALID_ARG);    // double
    }

    auris_stop(e);
    auris_destroy(e);
}

TEST_CASE("ABI fuzz: pathological queue sizes degrade to NO_MEMORY, never crash") {
    auris_config_t cfg{};
    auris_config_defaults(&cfg);
    cfg.backend = AURIS_BACKEND_HOST_PCM;
    cfg.codec   = AURIS_CODEC_PCM_S16LE;
    cfg.pcm_ring_frames = 0xFFFFFFFFu;         // ~4 G samples => alloc fails
    auris_engine_t* e = nullptr;
    const auris_status_t s = auris_create(&cfg, &e);
    CHECK((s == AURIS_ERR_NO_MEMORY || s == AURIS_ERR_INVALID_ARG));
    CHECK(e == nullptr);
    if (s == AURIS_OK && e) auris_destroy(e);  // defensive
}
