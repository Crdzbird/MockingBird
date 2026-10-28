// examples/smoke.cpp
//
// End-to-end PCM passthrough smoke test for the build-order step-1 skeleton.
//
// Proves:
//   * lifecycle (create/start/stop/destroy)
//   * the host-PCM push path into the SPSC ring
//   * the worker drains the ring, packetizes, and commits to the bounded queue
//   * the host drains, sees bytes intact, and releases without leaks
//   * SEGMENT_BEGIN / SEGMENT_END events fire on the pull-mode queue

#include "mockingbird/mockingbird.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace {
void die(const char* what, mockingbird_status_t s) {
    std::fprintf(stderr, "%s failed: %s\n", what, mockingbird_status_str(s));
    std::exit(1);
}

}

int main() {
    mockingbird_config_t cfg{};
    if (auto s = mockingbird_config_defaults(&cfg); s != MOCKINGBIRD_OK) die("defaults", s);

    cfg.backend            = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec              = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.sample_rate_hz     = 16000;
    cfg.frame_ms           = 20;
    cfg.enable_vad         = 0;
    cfg.enable_anomaly     = 0;
    cfg.vad_tier           = MOCKINGBIRD_VAD_FAST;
    cfg.vad_aggressiveness = 2;

    mockingbird_engine_t* engine = nullptr;
    if (auto s = mockingbird_create(&cfg, &engine); s != MOCKINGBIRD_OK) die("create", s);
    if (auto s = mockingbird_start(engine); s != MOCKINGBIRD_OK) die("start", s);

    const uint32_t frame_samples = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
    std::vector<int16_t> samples(frame_samples);
    for (uint32_t i = 0; i < frame_samples; ++i) {
        samples[i] = static_cast<int16_t>(i);
    }

    constexpr uint32_t kFramesToPush = 10;
    const uint64_t ts_base = 1'000'000;
    for (uint32_t f = 0; f < kFramesToPush; ++f) {
        const uint64_t ts = ts_base + uint64_t{f} * (uint64_t{cfg.frame_ms} * 1000ull);
        if (auto s = mockingbird_push_pcm(engine, samples.data(), frame_samples, ts); s != MOCKINGBIRD_OK) {
            die("push", s);
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint32_t drained        = 0;
    uint32_t pattern_errors = 0;
    bool     saw_keyframe   = false;
    while (true) {
        mockingbird_packet_t pkt{};
        const mockingbird_status_t ds = mockingbird_drain(engine, &pkt);
        if (ds == MOCKINGBIRD_ERR_QUEUE_EMPTY) break;
        if (ds != MOCKINGBIRD_OK) die("drain", ds);

        assert(pkt.codec == MOCKINGBIRD_CODEC_PCM_S16LE);
        assert(pkt.size == frame_samples * sizeof(int16_t));
        assert(pkt.data != nullptr);

        if (pkt.flags & MOCKINGBIRD_PKT_FLAG_KEYFRAME) saw_keyframe = true;

        const auto* p = reinterpret_cast<const int16_t*>(pkt.data);
        for (uint32_t i = 0; i < frame_samples; ++i) {
            if (p[i] != static_cast<int16_t>(i)) {
                ++pattern_errors;
                break;
            }
        }
        ++drained;

        if (auto s = mockingbird_release(engine, &pkt); s != MOCKINGBIRD_OK) die("release", s);
    }

    uint32_t segment_begins = 0;
    uint32_t segment_ends   = 0;
    while (true) {
        mockingbird_event_t evt{};
        const mockingbird_status_t es = mockingbird_poll_event(engine, &evt);
        if (es == MOCKINGBIRD_ERR_QUEUE_EMPTY) break;
        if (es != MOCKINGBIRD_OK) die("poll_event", es);
        if (evt.type == MOCKINGBIRD_EVT_SEGMENT_BEGIN) ++segment_begins;
        if (evt.type == MOCKINGBIRD_EVT_SEGMENT_END)   ++segment_ends;
    }

    mockingbird_stats_t stats{};
    if (auto s = mockingbird_get_stats(engine, &stats); s != MOCKINGBIRD_OK) die("get_stats", s);

    if (auto s = mockingbird_stop(engine); s != MOCKINGBIRD_OK) die("stop", s);

    while (true) {
        mockingbird_event_t evt{};
        const mockingbird_status_t es = mockingbird_poll_event(engine, &evt);
        if (es == MOCKINGBIRD_ERR_QUEUE_EMPTY) break;
        if (evt.type == MOCKINGBIRD_EVT_SEGMENT_END) ++segment_ends;
    }

    mockingbird_destroy(engine);

    std::printf("smoke: drained=%u pattern_errors=%u keyframe_seen=%s\n",
                drained, pattern_errors, saw_keyframe ? "yes" : "no");
    std::printf("smoke: segments begin=%u end=%u\n", segment_begins, segment_ends);
    std::printf("smoke: stats frames_captured=%llu packets_emitted=%llu "
                "packets_dropped=%llu frames_dropped_capture=%llu\n",
                static_cast<unsigned long long>(stats.frames_captured),
                static_cast<unsigned long long>(stats.packets_emitted),
                static_cast<unsigned long long>(stats.packets_dropped),
                static_cast<unsigned long long>(stats.frames_dropped_capture));

    bool ok = true;
    if (drained != kFramesToPush) {
        std::fprintf(stderr, "FAIL: expected %u packets, got %u\n",
                     kFramesToPush, drained);
        ok = false;
    }
    if (pattern_errors != 0) {
        std::fprintf(stderr, "FAIL: %u packets had mangled bytes\n", pattern_errors);
        ok = false;
    }
    if (!saw_keyframe) {
        std::fprintf(stderr, "FAIL: no keyframe flag on first packet\n");
        ok = false;
    }
    if (segment_begins != 1 || segment_ends != 1) {
        std::fprintf(stderr, "FAIL: expected 1 SEGMENT_BEGIN and 1 SEGMENT_END, got %u/%u\n",
                     segment_begins, segment_ends);
        ok = false;
    }
    if (stats.frames_dropped_capture != 0) {
        std::fprintf(stderr, "FAIL: %llu frames dropped at capture\n",
                     static_cast<unsigned long long>(stats.frames_dropped_capture));
        ok = false;
    }

    if (!ok) return 1;
    std::printf("smoke OK (mockingbird %s)\n", mockingbird_version_string());
    return 0;
}
