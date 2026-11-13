// tests/test_opus_conceal.cpp
//
// Packet loss concealment: a dropped packet is replaced by audio of the right
// length that carries the signal on, the stream stays close to the original
// across scattered losses, and the output room is respected exactly.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "opus_fixture.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

#ifdef MOCKINGBIRD_HAVE_OPUS

namespace {
double rms(const int16_t* v, size_t n) {
    double acc = 0.0;
    for (size_t i = 0; i < n; ++i) acc += static_cast<double>(v[i]) * v[i];
    return n == 0 ? 0.0 : std::sqrt(acc / static_cast<double>(n));
}

}

TEST_CASE("Opus conceal: dropped packets become audio of the packet's length") {
    const auto pcm  = opus_fixture::chord(16000, 32000);
    const auto pkts = opus_fixture::encode(pcm, 16000, 20, 32000);
    REQUIRE(pkts.size() == 100);
    mockingbird_decoder_t* d = nullptr;
    REQUIRE(mockingbird_decoder_create(16000, 1, &d) == MOCKINGBIRD_OK);
    std::vector<int16_t> out;
    std::vector<int16_t> frame(320);
    uint32_t concealed = 0;
    for (size_t i = 0; i < pkts.size(); ++i) {
        uint32_t n = 0;
        const bool lost = i >= 10 && i % 10 == 3;
        const mockingbird_status_t s = lost
            ? mockingbird_decoder_conceal(d, 0, frame.data(), 320, &n)
            : mockingbird_decoder_decode(d, pkts[i].bytes.data(), static_cast<uint32_t>(pkts[i].bytes.size()),
                                         frame.data(), 320, &n);
        REQUIRE(s == MOCKINGBIRD_OK);
        REQUIRE(n == 320);
        if (lost) {
            ++concealed;
            CHECK(rms(frame.data(), n) > 0.25 * rms(pcm.data() + i * 320, 320));
        }
        out.insert(out.end(), frame.begin(), frame.begin() + n);
    }
    mockingbird_decoder_destroy(d);
    CHECK(concealed == 9);
    CHECK(out.size() == pcm.size());
    const double snr = opus_fixture::best_snr_db(pcm, out, 16000);
    MESSAGE("SNR with 9% of packets concealed: " << snr << " dB");
    CHECK(snr >= 3.0);
}

TEST_CASE("Opus conceal: explicit durations, the 2.5 ms grid and the 120 ms cap") {
    mockingbird_decoder_t* d = nullptr;
    REQUIRE(mockingbird_decoder_create(48000, 1, &d) == MOCKINGBIRD_OK);
    std::vector<int16_t> buf(5760);
    uint32_t n = 0;
    CHECK(mockingbird_decoder_conceal(d, 0, buf.data(), 5760, &n) == MOCKINGBIRD_OK);
    CHECK(n == 960);
    for (uint32_t f : {120u, 480u, 2880u, 5760u}) {
        CHECK(mockingbird_decoder_conceal(d, f, buf.data(), 5760, &n) == MOCKINGBIRD_OK);
        CHECK(n == f);
    }
    for (uint32_t f : {1u, 119u, 121u, 5880u, 0xFFFFFFFFu}) {
        CHECK(mockingbird_decoder_conceal(d, f, buf.data(), 5760, &n) == MOCKINGBIRD_ERR_INVALID_ARG);
        CHECK(n == 0);
    }
    CHECK(mockingbird_decoder_conceal(d, 960, buf.data(), 959, &n) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_decoder_conceal(d, 960, nullptr, 960, &n) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_decoder_conceal(d, 960, buf.data(), 960, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    mockingbird_decoder_destroy(d);
}

TEST_CASE("Opus decode: a packet longer than the room is refused and nothing is written") {
    const auto pcm  = opus_fixture::chord(48000, 48000);
    const auto pkts = opus_fixture::encode(pcm, 48000, 20, 32000);
    REQUIRE(pkts.size() > 2);
    mockingbird_decoder_t* d = nullptr;
    REQUIRE(mockingbird_decoder_create(48000, 2, &d) == MOCKINGBIRD_OK);
    constexpr int16_t kCanary = 0x5A5A;
    std::vector<int16_t> buf(2 * 960 + 64, kCanary);
    uint32_t n = 0;
    const auto& p = pkts[1];
    const auto size = static_cast<uint32_t>(p.bytes.size());
    CHECK(mockingbird_decoder_decode(d, p.bytes.data(), size, buf.data(), 959, &n) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(n == 0);
    for (int16_t v : buf) REQUIRE(v == kCanary);
    CHECK(mockingbird_decoder_decode(d, p.bytes.data(), size, buf.data(), 960, &n) == MOCKINGBIRD_OK);
    CHECK(n == 960);
    for (size_t i = 2 * 960; i < buf.size(); ++i) REQUIRE(buf[i] == kCanary);
    CHECK(mockingbird_decoder_decode(d, p.bytes.data(), 0, buf.data(), 960, &n) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_decoder_decode(d, nullptr, size, buf.data(), 960, &n) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_decoder_decode(d, p.bytes.data(), size, nullptr, 960, &n) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_decoder_decode(d, p.bytes.data(), 65537, buf.data(), 960, &n) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_decoder_reset(d) == MOCKINGBIRD_OK);
    CHECK(mockingbird_decoder_decode(d, p.bytes.data(), size, buf.data(), 960, &n) == MOCKINGBIRD_OK);
    mockingbird_decoder_destroy(d);
}

#endif
