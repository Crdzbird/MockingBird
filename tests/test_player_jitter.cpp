// tests/test_player_jitter.cpp
//
// The jitter buffer through a HOST_PCM player: in order or shuffled, the
// output is the decoder's output sample for sample; a missing packet is
// concealed in its exact place; late and duplicate packets are dropped; the
// first packet waits the target delay; an underrun fades and rebuffers.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "player_fixture.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

#ifdef MOCKINGBIRD_HAVE_OPUS

using namespace player_fixture;

TEST_CASE("player: packets in order play exactly what the decoder produces") {
    const auto pkts = packets();
    mockingbird_player_t* p = make(60, 10000);
    REQUIRE(p != nullptr);
    for (size_t i = 0; i < pkts.size(); ++i) {
        push(p, pkts[i], i * kFrameUs, i + 1 == pkts.size() ? MOCKINGBIRD_PKT_FLAG_LAST : 0);
    }
    const auto out = read(p, static_cast<uint32_t>(pkts.size()) * kFrame);
    CHECK(out == reference(pkts, {}));
    const auto s = stats(p);
    CHECK(s.packets_received == pkts.size());
    CHECK(s.packets_played == pkts.size());
    CHECK(s.packets_concealed == 0);
    CHECK(s.packets_dropped == 0);
    CHECK(s.underruns == 0);
    read(p, 1600);
    CHECK(stats(p).state == MOCKINGBIRD_PLAYER_IDLE);
    CHECK(stats(p).underruns == 0);
    mockingbird_player_destroy(p);
}

TEST_CASE("player: packets shuffled within the delay come out in pts order") {
    const auto pkts = packets();
    mockingbird_player_t* p = make(60, 10000);
    std::vector<size_t> order(pkts.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    for (size_t i = 0; i + 1 < order.size(); i += 3) std::swap(order[i], order[i + 1]);
    for (size_t i : order) push(p, pkts[i], i * kFrameUs);
    CHECK(read(p, static_cast<uint32_t>(pkts.size()) * kFrame) == reference(pkts, {}));
    CHECK(stats(p).packets_dropped == 0);
    mockingbird_player_destroy(p);
}

TEST_CASE("player: a missing packet is concealed in its place and the next one plays on time") {
    const auto pkts = packets();
    mockingbird_player_t* p = make(60, 10000);
    for (size_t i = 0; i < pkts.size(); ++i) {
        if (i != 17 && i != 40 && i != 41) push(p, pkts[i], i * kFrameUs);
    }
    CHECK(read(p, static_cast<uint32_t>(pkts.size()) * kFrame) == reference(pkts, {17, 40, 41}));
    CHECK(stats(p).packets_concealed == 3);
    mockingbird_player_destroy(p);
}

TEST_CASE("player: late and duplicate packets are dropped and counted") {
    const auto pkts = packets(1);
    mockingbird_player_t* p = make(0, 10000);
    for (size_t i = 0; i < 10; ++i) {
        if (i != 4) push(p, pkts[i], i * kFrameUs);
    }
    read(p, 8 * kFrame);
    push(p, pkts[4], 4 * kFrameUs);
    push(p, pkts[9], 9 * kFrameUs);
    read(p, 160);
    const auto s = stats(p);
    CHECK(s.packets_concealed == 1);
    CHECK(s.packets_late == 2);
    CHECK(s.packets_dropped == 2);
    mockingbird_player_destroy(p);
}

TEST_CASE("player: the first packet waits the target delay") {
    const auto pkts = packets(1);
    mockingbird_player_t* p = make(60, 400);
    push(p, pkts[0], 0);
    const auto out = read(p, 1600);
    CHECK(std::all_of(out.begin(), out.begin() + 960, [](int16_t v) { return v == 0; }));
    CHECK(stats(p).packets_played == 1);
    mockingbird_player_destroy(p);
}

TEST_CASE("player: an underrun fades out once, rebuffers and resumes on the same timeline") {
    const auto pkts = packets(1);
    mockingbird_player_t* p = make(40, 400);
    for (size_t i = 0; i < 10; ++i) push(p, pkts[i], i * kFrameUs);
    read(p, 10 * kFrame);
    CHECK(stats(p).state == MOCKINGBIRD_PLAYER_PLAYING);
    read(p, 6 * kFrame);
    auto s = stats(p);
    CHECK(s.underruns == 1);
    CHECK(s.state == MOCKINGBIRD_PLAYER_BUFFERING);
    for (size_t i = 10; i < 20; ++i) push(p, pkts[i], i * kFrameUs);
    read(p, 10 * kFrame);
    s = stats(p);
    CHECK(s.packets_played == 20);
    CHECK(s.packets_dropped == 0);
    CHECK(s.underruns == 1);
    mockingbird_player_destroy(p);
}

#endif
