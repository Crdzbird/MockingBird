// tests/test_opus_decoder.cpp
//
// Round trips through the engine's own Opus encoder and the public decoder at
// every rate and frame size the engine can encode, with a measured
// signal-to-noise floor, plus the decoder's argument checks.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "opus_fixture.hpp"

#include <cstdint>
#include <vector>

#ifdef MOCKINGBIRD_HAVE_OPUS

namespace {
std::vector<int16_t> decode_all(const std::vector<opus_fixture::Packet>& pkts, uint32_t rate) {
    mockingbird_decoder_t* d = nullptr;
    REQUIRE(mockingbird_decoder_create(rate, 1, &d) == MOCKINGBIRD_OK);
    std::vector<int16_t> out;
    std::vector<int16_t> frame(5760);
    for (const auto& p : pkts) {
        uint32_t n = 0;
        REQUIRE(mockingbird_decoder_decode(d, p.bytes.data(), static_cast<uint32_t>(p.bytes.size()),
                                           frame.data(), 5760, &n) == MOCKINGBIRD_OK);
        out.insert(out.end(), frame.begin(), frame.begin() + n);
    }
    mockingbird_decoder_destroy(d);
    return out;
}

}

TEST_CASE("Opus round trip through the engine encoder at every rate and frame size") {
    const uint32_t rates[]  = {8000, 16000, 48000};
    const uint32_t frames[] = {10, 20};
    for (uint32_t rate : rates) {
        for (uint32_t ms : frames) {
            const auto pcm  = opus_fixture::chord(rate, rate * 2u);
            const auto pkts = opus_fixture::encode(pcm, rate, ms, 64000);
            REQUIRE(pkts.size() == pcm.size() / (rate / 1000u * ms));
            const auto out = decode_all(pkts, rate);
            CHECK(out.size() == pcm.size());
            uint32_t delay = 0;
            const double snr = opus_fixture::best_snr_db(pcm, out, rate, &delay);
            MESSAGE("rate " << rate << " frame " << ms << " ms: SNR " << snr << " dB, delay " << delay);
            CHECK(snr >= 10.0);
        }
    }
}

TEST_CASE("Opus round trip: a mono stream decodes on a stereo decoder as two equal channels") {
    const auto pcm  = opus_fixture::chord(16000, 16000);
    const auto pkts = opus_fixture::encode(pcm, 16000, 20, 32000);
    REQUIRE(!pkts.empty());
    mockingbird_decoder_t* d = nullptr;
    REQUIRE(mockingbird_decoder_create(16000, 2, &d) == MOCKINGBIRD_OK);
    std::vector<int16_t> out(640);
    uint32_t n = 0;
    for (const auto& p : pkts) {
        REQUIRE(mockingbird_decoder_decode(d, p.bytes.data(), static_cast<uint32_t>(p.bytes.size()),
                                           out.data(), 320, &n) == MOCKINGBIRD_OK);
        REQUIRE(n == 320);
        for (uint32_t i = 0; i < n; ++i) REQUIRE(out[2 * i] == out[2 * i + 1]);
    }
    mockingbird_decoder_destroy(d);
}

TEST_CASE("Opus decoder: create rejects bad rates, channels and null out-params") {
    mockingbird_decoder_t* d = reinterpret_cast<mockingbird_decoder_t*>(0x1);
    CHECK(mockingbird_decoder_create(16000, 1, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    for (uint32_t rate : {0u, 11025u, 22050u, 32000u, 44100u, 96000u}) {
        CHECK(mockingbird_decoder_create(rate, 1, &d) == MOCKINGBIRD_ERR_INVALID_ARG);
        CHECK(d == nullptr);
    }
    for (uint16_t ch : {uint16_t{0}, uint16_t{3}, uint16_t{255}}) {
        CHECK(mockingbird_decoder_create(48000, ch, &d) == MOCKINGBIRD_ERR_INVALID_ARG);
        CHECK(d == nullptr);
    }
    for (uint32_t rate : {8000u, 12000u, 16000u, 24000u, 48000u}) {
        REQUIRE(mockingbird_decoder_create(rate, 2, &d) == MOCKINGBIRD_OK);
        mockingbird_decoder_destroy(d);
    }
    mockingbird_decoder_destroy(nullptr);
    uint32_t n = 7;
    int16_t  buf[4];
    const uint8_t one = 0;
    CHECK(mockingbird_decoder_decode(nullptr, &one, 1, buf, 4, &n) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(n == 0);
    CHECK(mockingbird_decoder_conceal(nullptr, 0, buf, 4, &n) == MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_decoder_reset(nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
}

#else

TEST_CASE("Opus decoder: a build without libopus reports UNSUPPORTED") {
    mockingbird_decoder_t* d = nullptr;
    CHECK(mockingbird_decoder_create(16000, 1, &d) == MOCKINGBIRD_ERR_UNSUPPORTED);
    CHECK(d == nullptr);
}

#endif
