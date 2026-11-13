// tests/opus_fixture.hpp
//
// Shared by the decoder and player tests: a test signal, the engine's own
// Opus encoder run through the public ABI, and a signal-to-noise measure
// that finds the codec delay itself.

#pragma once

#include "mockingbird/mockingbird.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace opus_fixture {
struct Packet {
    std::vector<uint8_t> bytes;
    uint64_t             pts_us = 0;
    uint32_t             flags  = 0;
};

inline std::vector<int16_t> chord(uint32_t rate, uint32_t samples) {
    constexpr double kPi = 3.14159265358979323846;
    std::vector<int16_t> v(samples);
    for (uint32_t i = 0; i < samples; ++i) {
        const double t  = static_cast<double>(i) / rate;
        const double am = 0.75 + 0.25 * std::sin(2.0 * kPi * 3.0 * t);
        const double s  = std::sin(2.0 * kPi * 330.0 * t) + 0.6 * std::sin(2.0 * kPi * 1210.0 * t) +
                         0.3 * std::sin(2.0 * kPi * 2470.0 * t);
        v[i] = static_cast<int16_t>(std::lround(0.18 * 32767.0 * am * s));
    }
    return v;
}

inline void drain_into(mockingbird_engine_t* e, std::vector<Packet>& out) {
    mockingbird_packet_t pkt{};
    while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) {
        out.push_back({std::vector<uint8_t>(pkt.data, pkt.data + pkt.size), pkt.capture_ts_us, pkt.flags});
        mockingbird_release(e, &pkt);
    }
}

inline std::vector<Packet> encode(const std::vector<int16_t>& pcm, uint32_t rate, uint32_t frame_ms,
                                  uint32_t bitrate) {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend               = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec                 = MOCKINGBIRD_CODEC_OPUS;
    cfg.sample_rate_hz        = rate;
    cfg.frame_ms              = frame_ms;
    cfg.opus_bitrate_bps      = bitrate;
    cfg.enable_vad            = 0;
    cfg.enable_anomaly        = 0;
    cfg.pcm_ring_frames       = 1u << 20;
    cfg.packet_queue_capacity = 8192;
    std::vector<Packet> out;
    mockingbird_engine_t* e = nullptr;
    if (mockingbird_create(&cfg, &e) != MOCKINGBIRD_OK) return out;
    mockingbird_start(e);
    const uint32_t frame = rate / 1000u * frame_ms;
    for (uint32_t at = 0; at + frame <= pcm.size(); at += frame) {
        mockingbird_push_pcm(e, pcm.data() + at, frame, uint64_t{at} * 1'000'000u / rate);
        drain_into(e, out);
    }
    mockingbird_stop(e);
    drain_into(e, out);
    mockingbird_destroy(e);
    return out;
}

inline double snr_db(const std::vector<int16_t>& ref, const std::vector<int16_t>& got, uint32_t delay,
                     size_t from, size_t to) {
    double sig = 0.0;
    double err = 0.0;
    for (size_t i = from; i < to && i + delay < got.size() && i < ref.size(); ++i) {
        const double x = ref[i];
        const double d = x - got[i + delay];
        sig += x * x;
        err += d * d;
    }
    return err <= 0.0 ? 200.0 : 10.0 * std::log10(sig / err);
}

inline double best_snr_db(const std::vector<int16_t>& ref, const std::vector<int16_t>& got, uint32_t rate,
                          uint32_t* found_delay = nullptr) {
    const size_t from = rate / 5u;
    const size_t to   = ref.size() - rate / 10u;
    double best = -200.0;
    for (uint32_t d = 0; d <= rate / 25u; ++d) {
        const double s = snr_db(ref, got, d, from, to);
        if (s > best) {
            best = s;
            if (found_delay != nullptr) *found_delay = d;
        }
    }
    return best;
}

}
