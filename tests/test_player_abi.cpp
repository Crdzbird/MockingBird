// tests/test_player_abi.cpp
//
// The player's boundary: every config field and push argument is validated,
// calls in the wrong state return their status, and random bytes, random
// pts and random reads never crash it or break its counters.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "player_fixture.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
mockingbird_player_config_t defaults() {
    mockingbird_player_config_t c{};
    REQUIRE(mockingbird_player_config_defaults(&c) == MOCKINGBIRD_OK);
    c.backend = MOCKINGBIRD_BACKEND_HOST_PCM;
    return c;
}

mockingbird_status_t create_with(const mockingbird_player_config_t& c) {
    mockingbird_player_t* p = reinterpret_cast<mockingbird_player_t*>(0x1);
    const mockingbird_status_t s = mockingbird_player_create(&c, &p);
    if (s == MOCKINGBIRD_OK) mockingbird_player_destroy(p);
    if (s != MOCKINGBIRD_OK) REQUIRE(p == nullptr);
    return s;
}

}

TEST_CASE("player ABI: config defaults and every invalid field") {
    CHECK(mockingbird_player_config_defaults(nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    auto c = defaults();
    CHECK(c.sample_rate_hz == 16000);
    CHECK(c.channels == 1);
    CHECK(create_with(c) == MOCKINGBIRD_OK);
    CHECK(mockingbird_player_create(nullptr, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    mockingbird_player_t* p = nullptr;
    CHECK(mockingbird_player_create(nullptr, &p) == MOCKINGBIRD_ERR_INVALID_ARG);
    auto bad = [&](auto&& edit) {
        auto x = defaults();
        edit(x);
        return create_with(x);
    };
    CHECK(bad([](auto& x) { x.backend = static_cast<mockingbird_backend_t>(9); }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.sample_rate_hz = 44100; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.sample_rate_hz = 32000; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.channels = 0; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.channels = 3; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.target_delay_ms = 2001; x.max_delay_ms = 3000; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.max_delay_ms = x.target_delay_ms - 1; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.max_delay_ms = 10001; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.conceal_max_ms = 1001; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.queue_packets = 3; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.queue_packets = 4097; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x._reserved[2] = 1; }) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(bad([](auto& x) { x.sample_rate_hz = 48000; x.channels = 2; x.target_delay_ms = 0; }) == MOCKINGBIRD_OK);
}

TEST_CASE("player ABI: push, read and controls reject bad arguments and wrong states") {
    auto c = defaults();
    mockingbird_player_t* p = nullptr;
    REQUIRE(mockingbird_player_create(&c, &p) == MOCKINGBIRD_OK);
    const uint8_t pcm[640] = {};
    int16_t out[320];
    CHECK(mockingbird_player_push(nullptr, MOCKINGBIRD_CODEC_PCM_S16LE, pcm, 640, 0, 0) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_push(p, MOCKINGBIRD_CODEC_PCM_S16LE, nullptr, 640, 0, 0) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_push(p, MOCKINGBIRD_CODEC_PCM_S16LE, pcm, 0, 0, 0) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_push(p, MOCKINGBIRD_CODEC_PCM_S16LE, pcm, 639, 0, 0) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_push(p, MOCKINGBIRD_CODEC_PCM_S16LE, pcm, 8193, 0, 0) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_push(p, MOCKINGBIRD_CODEC_OGG_OPUS, pcm, 640, 0, 0) == MOCKINGBIRD_ERR_UNSUPPORTED);
    CHECK(mockingbird_player_push(p, MOCKINGBIRD_CODEC_FLAC, pcm, 640, 0, 0) == MOCKINGBIRD_ERR_UNSUPPORTED);
    CHECK(mockingbird_player_push(p, static_cast<mockingbird_codec_t>(77), pcm, 640, 0, 0) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_push(p, MOCKINGBIRD_CODEC_PCM_S16LE, pcm, 640, 0, 0) == MOCKINGBIRD_OK);
    std::vector<uint8_t> long_pcm(2u * 16000u * 121u / 1000u + 2u);
    CHECK(mockingbird_player_push(p, MOCKINGBIRD_CODEC_PCM_S16LE, long_pcm.data(),
                                  static_cast<uint32_t>(long_pcm.size()), 0, 0) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_read(p, out, 160) == MOCKINGBIRD_ERR_NOT_RUNNING);
    CHECK(mockingbird_player_pause(p) == MOCKINGBIRD_ERR_NOT_RUNNING);
    CHECK(mockingbird_player_resume(p) == MOCKINGBIRD_ERR_NOT_RUNNING);
    CHECK(mockingbird_player_stop(p) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_player_start(p) == MOCKINGBIRD_OK);
    CHECK(mockingbird_player_start(p) == MOCKINGBIRD_ERR_ALREADY_RUNNING);
    CHECK(mockingbird_player_read(p, nullptr, 160) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_read(p, out, 16001) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_read(p, out, 0) == MOCKINGBIRD_OK);
    CHECK(mockingbird_player_set_volume(p, -0.1f) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_set_volume(p, 1.01f) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_set_volume(p, std::numeric_limits<float>::quiet_NaN()) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_set_volume(nullptr, 1.0f) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_set_muted(nullptr, 1) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_get_stats(p, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_get_stats(nullptr, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_flush(nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_start(nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    mockingbird_player_destroy(p);
    mockingbird_player_destroy(nullptr);

    auto d = defaults();
    d.backend = MOCKINGBIRD_BACKEND_MINIAUDIO;
    REQUIRE(mockingbird_player_create(&d, &p) == MOCKINGBIRD_OK);
    CHECK(mockingbird_player_read(p, out, 160) == MOCKINGBIRD_ERR_INVALID_STATE);
    mockingbird_player_destroy(p);
}

TEST_CASE("ABI fuzz: random packets, pts and reads never crash the player or skew its counts") {
    struct Rng {
        uint64_t s = 0xB1A5u;
        uint32_t u32() {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            return static_cast<uint32_t>(s);
        }
    } r;
    auto c = defaults();
    c.queue_packets = 16;
    mockingbird_player_t* p = nullptr;
    REQUIRE(mockingbird_player_create(&c, &p) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_player_start(p) == MOCKINGBIRD_OK);
    const auto real = player_fixture::packets(1);
    std::vector<uint8_t> bytes(8192);
    std::vector<int16_t> out(16000);
    uint64_t accepted = 0;
    for (int i = 0; i < 20000; ++i) {
        const uint32_t pick = r.u32() % 4;
        const auto& pkt = real[r.u32() % real.size()].bytes;
        uint32_t size = 1 + r.u32() % 8192;
        for (uint32_t k = 0; k < size; ++k) bytes[k] = static_cast<uint8_t>(r.u32());
        if (pick == 0) {
            size = static_cast<uint32_t>(pkt.size());
            std::copy(pkt.begin(), pkt.end(), bytes.begin());
            bytes[r.u32() % size] ^= static_cast<uint8_t>(r.u32());
        }
        const auto codec = pick == 3 ? MOCKINGBIRD_CODEC_PCM_S16LE : MOCKINGBIRD_CODEC_OPUS;
        const uint64_t pts = uint64_t{r.u32()} * (r.u32() % 3 == 0 ? 1000u : 1u);
        const mockingbird_status_t s = mockingbird_player_push(p, codec, bytes.data(), size, pts, r.u32());
        REQUIRE((s == MOCKINGBIRD_OK || s == MOCKINGBIRD_ERR_INVALID_ARG));
        if (s == MOCKINGBIRD_OK) ++accepted;
        if (i % 7 == 0) REQUIRE(mockingbird_player_read(p, out.data(), r.u32() % 16001) == MOCKINGBIRD_OK);
        if (i % 997 == 0) mockingbird_player_flush(p);
    }
    const auto st = player_fixture::stats(p);
    CHECK(st.packets_received + st.packets_dropped >= accepted);
    CHECK(st.buffered_packets <= 16);
    mockingbird_player_destroy(p);
}
