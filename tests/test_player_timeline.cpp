// tests/test_player_timeline.cpp
//
// The player's timeline rules: a backlog past max_delay_ms is cut to the
// target, a long hole is silence between talkspurts rather than
// concealment, a sender restart or a jump far ahead starts a new timeline,
// flush and stop drop what is buffered, PCM plays as pushed, and volume and
// mute scale it.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "player_fixture.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

using namespace player_fixture;

namespace {
std::vector<uint8_t> pcm_bytes(int16_t value, uint32_t frames) {
    std::vector<int16_t> v(frames, value);
    const auto* b = reinterpret_cast<const uint8_t*>(v.data());
    return std::vector<uint8_t>(b, b + v.size() * sizeof(int16_t));
}

void push_pcm(mockingbird_player_t* p, int16_t value, uint64_t pts) {
    const auto b = pcm_bytes(value, kFrame);
    REQUIRE(mockingbird_player_push(p, MOCKINGBIRD_CODEC_PCM_S16LE, b.data(), static_cast<uint32_t>(b.size()),
                                    pts, 0) == MOCKINGBIRD_OK);
}

}

TEST_CASE("player: PCM packets play exactly as pushed, in pts order") {
    mockingbird_player_t* p = make(0, 400);
    push_pcm(p, 200, kFrameUs);
    push_pcm(p, 100, 0);
    const auto out = read(p, 2 * kFrame);
    CHECK(std::all_of(out.begin(), out.begin() + kFrame, [](int16_t v) { return v == 100; }));
    CHECK(std::all_of(out.begin() + kFrame, out.end(), [](int16_t v) { return v == 200; }));
    mockingbird_player_destroy(p);
}

TEST_CASE("player: a backlog past max_delay_ms is cut back to the target") {
    mockingbird_player_t* p = make(60, 200);
    for (uint64_t i = 0; i < 50; ++i) push_pcm(p, 1000, i * kFrameUs);
    read(p, 160);
    const auto s = stats(p);
    CHECK(s.buffered_ms <= 200);
    CHECK(s.packets_dropped > 0);
    CHECK(s.packets_received == 50);
    mockingbird_player_destroy(p);
}

TEST_CASE("player: a hole longer than conceal_max_ms is a new talkspurt, not concealment") {
    mockingbird_player_t* p = make(0, 400, 100);
    for (uint64_t i = 0; i < 5; ++i) push_pcm(p, 500, i * kFrameUs);
    for (uint64_t i = 40; i < 45; ++i) push_pcm(p, 700, i * kFrameUs);
    const auto out = read(p, 10 * kFrame);
    CHECK(std::all_of(out.begin(), out.end(), [](int16_t v) { return v != 0; }));
    CHECK(stats(p).packets_concealed == 0);
    CHECK(stats(p).packets_played == 10);
    mockingbird_player_destroy(p);
}

TEST_CASE("player: a pts far behind the playout point starts a new timeline") {
    mockingbird_player_t* p = make(0, 10000);
    for (uint64_t i = 200; i < 205; ++i) push_pcm(p, 300, i * kFrameUs);
    read(p, 5 * kFrame);
    for (uint64_t i = 0; i < 5; ++i) push_pcm(p, 900, i * kFrameUs);
    const auto out = read(p, 5 * kFrame);
    CHECK(std::all_of(out.begin(), out.end(), [](int16_t v) { return v == 900; }));
    CHECK(stats(p).packets_late == 0);
    mockingbird_player_destroy(p);
}

TEST_CASE("player: a pts far ahead of the playout point plays as a new talkspurt") {
    mockingbird_player_t* p = make(0, 10000);
    for (uint64_t i = 0; i < 5; ++i) push_pcm(p, 300, i * kFrameUs);
    read(p, 5 * kFrame);
    const uint64_t epoch_us = 1'700'000'000'000'000;
    for (uint64_t i = 0; i < 5; ++i) push_pcm(p, 900, epoch_us + i * kFrameUs);
    const auto out = read(p, 5 * kFrame);
    CHECK(std::all_of(out.begin(), out.end(), [](int16_t v) { return v == 900; }));
    CHECK(stats(p).packets_concealed == 0);
    mockingbird_player_destroy(p);
}

TEST_CASE("player: flush and stop drop what is buffered; pushes after them play") {
    mockingbird_player_t* p = make(0, 10000);
    for (uint64_t i = 0; i < 5; ++i) push_pcm(p, 111, i * kFrameUs);
    REQUIRE(mockingbird_player_flush(p) == MOCKINGBIRD_OK);
    push_pcm(p, 222, 0);
    auto out = read(p, kFrame);
    CHECK(std::all_of(out.begin(), out.end(), [](int16_t v) { return v == 222; }));
    push_pcm(p, 333, kFrameUs);
    REQUIRE(mockingbird_player_stop(p) == MOCKINGBIRD_OK);
    CHECK(stats(p).state == MOCKINGBIRD_PLAYER_STOPPED);
    int16_t one[160];
    CHECK(mockingbird_player_read(p, one, 160) == MOCKINGBIRD_ERR_NOT_RUNNING);
    REQUIRE(mockingbird_player_start(p) == MOCKINGBIRD_OK);
    out = read(p, kFrame);
    CHECK(std::all_of(out.begin(), out.end(), [](int16_t v) { return v == 0; }));
    mockingbird_player_destroy(p);
}

TEST_CASE("player: pause holds playout, resume continues, volume and mute scale the output") {
    mockingbird_player_t* p = make(0, 10000);
    for (uint64_t i = 0; i < 20; ++i) push_pcm(p, 10000, i * kFrameUs);
    read(p, kFrame);
    REQUIRE(mockingbird_player_pause(p) == MOCKINGBIRD_OK);
    CHECK(stats(p).state == MOCKINGBIRD_PLAYER_PAUSED);
    auto out = read(p, kFrame);
    CHECK(std::all_of(out.begin(), out.end(), [](int16_t v) { return v == 0; }));
    REQUIRE(mockingbird_player_resume(p) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_player_set_volume(p, 0.5f) == MOCKINGBIRD_OK);
    out = read(p, 2 * kFrame);
    CHECK(out.back() == 5000);
    REQUIRE(mockingbird_player_set_muted(p, 1) == MOCKINGBIRD_OK);
    out = read(p, 2 * kFrame);
    CHECK(out.back() == 0);
    REQUIRE(mockingbird_player_set_muted(p, 0) == MOCKINGBIRD_OK);
    out = read(p, 2 * kFrame);
    CHECK(out.back() == 5000);
    CHECK(stats(p).packets_played == 7);
    mockingbird_player_destroy(p);
}
