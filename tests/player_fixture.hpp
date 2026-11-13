// tests/player_fixture.hpp
//
// A HOST_PCM player read in 10 ms steps, and the reference the jitter tests
// compare it with: the same packets through a bare decoder, with concealment
// where a packet is meant to be missing.

#pragma once

#include "mockingbird/mockingbird.h"
#include "opus_fixture.hpp"

#include <cstdint>
#include <set>
#include <vector>

namespace player_fixture {
constexpr uint32_t kRate  = 16000;
constexpr uint32_t kFrame = 320;
constexpr uint64_t kFrameUs = 20000;

inline mockingbird_player_t* make(uint32_t target_ms = 60, uint32_t max_ms = 400, uint32_t conceal_ms = 100) {
    mockingbird_player_config_t cfg{};
    mockingbird_player_config_defaults(&cfg);
    cfg.backend         = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.sample_rate_hz  = kRate;
    cfg.target_delay_ms = target_ms;
    cfg.max_delay_ms    = max_ms;
    cfg.conceal_max_ms  = conceal_ms;
    cfg.queue_packets   = 256;
    mockingbird_player_t* p = nullptr;
    if (mockingbird_player_create(&cfg, &p) != MOCKINGBIRD_OK) return nullptr;
    mockingbird_player_start(p);
    return p;
}

inline std::vector<opus_fixture::Packet> packets(uint32_t seconds = 2) {
    return opus_fixture::encode(opus_fixture::chord(kRate, kRate * seconds), kRate, 20, 32000);
}

inline void push(mockingbird_player_t* p, const opus_fixture::Packet& pkt, uint64_t pts, uint32_t flags = 0) {
    mockingbird_player_push(p, MOCKINGBIRD_CODEC_OPUS, pkt.bytes.data(), static_cast<uint32_t>(pkt.bytes.size()),
                            pts, flags);
}

inline std::vector<int16_t> read(mockingbird_player_t* p, uint32_t frames) {
    std::vector<int16_t> out(frames);
    for (uint32_t at = 0; at < frames; at += 160) mockingbird_player_read(p, out.data() + at, 160);
    return out;
}

inline mockingbird_player_stats_t stats(mockingbird_player_t* p) {
    mockingbird_player_stats_t s{};
    mockingbird_player_get_stats(p, &s);
    return s;
}

inline std::vector<int16_t> reference(const std::vector<opus_fixture::Packet>& pkts,
                                      const std::set<size_t>& lost) {
    mockingbird_decoder_t* d = nullptr;
    mockingbird_decoder_create(kRate, 1, &d);
    std::vector<int16_t> out;
    std::vector<int16_t> frame(kFrame);
    for (size_t i = 0; i < pkts.size(); ++i) {
        uint32_t n = 0;
        if (lost.count(i) != 0) {
            mockingbird_decoder_conceal(d, kFrame, frame.data(), kFrame, &n);
        } else {
            mockingbird_decoder_decode(d, pkts[i].bytes.data(), static_cast<uint32_t>(pkts[i].bytes.size()),
                                       frame.data(), kFrame, &n);
        }
        out.insert(out.end(), frame.begin(), frame.begin() + n);
    }
    mockingbird_decoder_destroy(d);
    return out;
}

inline size_t first_sound(const std::vector<int16_t>& v) {
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] != 0) return i;
    }
    return v.size();
}

}
