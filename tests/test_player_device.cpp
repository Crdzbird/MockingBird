// tests/test_player_device.cpp
//
// Plays through the machine's real output, muted, on every device backend
// this build has. Opt-in with MOCKINGBIRD_TEST_AUDIO_OUTPUT=1, because CI
// runners have no output device; the case reports a skip otherwise.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "player_fixture.hpp"

#include <chrono>
#include <cstdlib>
#include <thread>

#ifdef MOCKINGBIRD_HAVE_OPUS

namespace {
void play_on(mockingbird_backend_t backend) {
    mockingbird_player_config_t c{};
    mockingbird_player_config_defaults(&c);
    c.backend      = backend;
    c.max_delay_ms = 2000;
    mockingbird_player_t* p = nullptr;
    REQUIRE(mockingbird_player_create(&c, &p) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_player_set_muted(p, 1) == MOCKINGBIRD_OK);
    const auto pkts = player_fixture::packets(1);
    REQUIRE(mockingbird_player_start(p) == MOCKINGBIRD_OK);
    for (size_t i = 0; i < pkts.size(); ++i) player_fixture::push(p, pkts[i], i * player_fixture::kFrameUs);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    REQUIRE(mockingbird_player_pause(p) == MOCKINGBIRD_OK);
    CHECK(player_fixture::stats(p).state == MOCKINGBIRD_PLAYER_PAUSED);
    REQUIRE(mockingbird_player_resume(p) == MOCKINGBIRD_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(900));
    const auto s = player_fixture::stats(p);
    MESSAGE("backend " << static_cast<int>(backend) << ": played " << s.packets_played << " of " << pkts.size()
                       << ", rendered " << s.frames_rendered << " frames, underruns " << s.underruns);
    CHECK(s.packets_played == pkts.size());
    CHECK(s.frames_rendered >= 16000);
    REQUIRE(mockingbird_player_stop(p) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_player_start(p) == MOCKINGBIRD_OK);
    mockingbird_player_destroy(p);
}

}

TEST_CASE("player device: plays through the real output when MOCKINGBIRD_TEST_AUDIO_OUTPUT=1") {
    const char* on = std::getenv("MOCKINGBIRD_TEST_AUDIO_OUTPUT");
    if (on == nullptr || on[0] != '1') {
        MESSAGE("no output device test requested, skip");
        return;
    }
    play_on(MOCKINGBIRD_BACKEND_AUTO);
    play_on(MOCKINGBIRD_BACKEND_MINIAUDIO);
}

#endif
