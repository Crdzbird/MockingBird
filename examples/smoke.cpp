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

#include "auris/auris.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace {

void die(const char* what, auris_status_t s) {
    std::fprintf(stderr, "%s failed: %s\n", what, auris_status_str(s));
    std::exit(1);
}

} // namespace

int main() {
    auris_config_t cfg{};
    if (auto s = auris_config_defaults(&cfg); s != AURIS_OK) die("defaults", s);

    cfg.backend            = AURIS_BACKEND_HOST_PCM;
    cfg.codec              = AURIS_CODEC_PCM_S16LE;   // passthrough
    cfg.sample_rate_hz     = 16000;
    cfg.frame_ms           = 20;                      // 320 samples per frame
    // Smoke pushes a synthetic sawtooth that VAD would classify as silence.
    // Run in passthrough mode (no detection) so the queue-and-drain plumbing
    // is verified in isolation. Live VAD behavior is exercised by the
    // Flutter demo on iOS / Android.
    cfg.enable_vad         = 0;
    cfg.enable_anomaly     = 0;
    cfg.vad_tier           = AURIS_VAD_FAST;
    cfg.vad_aggressiveness = 2;

    auris_engine_t* engine = nullptr;
    if (auto s = auris_create(&cfg, &engine); s != AURIS_OK) die("create", s);
    if (auto s = auris_start(engine); s != AURIS_OK) die("start", s);

    const uint32_t frame_samples = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
    std::vector<int16_t> samples(frame_samples);
    for (uint32_t i = 0; i < frame_samples; ++i) {
        samples[i] = static_cast<int16_t>(i);   // recognizable pattern
    }

    constexpr uint32_t kFramesToPush = 10;
    const uint64_t ts_base = 1'000'000;          // arbitrary monotonic origin
    for (uint32_t f = 0; f < kFramesToPush; ++f) {
        const uint64_t ts = ts_base + uint64_t{f} * (uint64_t{cfg.frame_ms} * 1000ull);
        if (auto s = auris_push_pcm(engine, samples.data(), frame_samples, ts); s != AURIS_OK) {
            die("push", s);
        }
    }

    // Give the worker time to drain the ring into the packet queue.
    // The worker wakes on the wake_count notify, but the smoke is single-
    // threaded on the host side so a small sleep beats a busy-poll here.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Drain packets, verify bytes.
    uint32_t drained        = 0;
    uint32_t pattern_errors = 0;
    bool     saw_keyframe   = false;
    while (true) {
        auris_packet_t pkt{};
        const auris_status_t ds = auris_drain(engine, &pkt);
        if (ds == AURIS_ERR_QUEUE_EMPTY) break;
        if (ds != AURIS_OK) die("drain", ds);

        assert(pkt.codec == AURIS_CODEC_PCM_S16LE);
        assert(pkt.size == frame_samples * sizeof(int16_t));
        assert(pkt.data != nullptr);

        if (pkt.flags & AURIS_PKT_FLAG_KEYFRAME) saw_keyframe = true;

        const auto* p = reinterpret_cast<const int16_t*>(pkt.data);
        for (uint32_t i = 0; i < frame_samples; ++i) {
            if (p[i] != static_cast<int16_t>(i)) {
                ++pattern_errors;
                break;
            }
        }
        ++drained;

        if (auto s = auris_release(engine, &pkt); s != AURIS_OK) die("release", s);
    }

    // Pull events.
    uint32_t segment_begins = 0;
    uint32_t segment_ends   = 0;
    while (true) {
        auris_event_t evt{};
        const auris_status_t es = auris_poll_event(engine, &evt);
        if (es == AURIS_ERR_QUEUE_EMPTY) break;
        if (es != AURIS_OK) die("poll_event", es);
        if (evt.type == AURIS_EVT_SEGMENT_BEGIN) ++segment_begins;
        if (evt.type == AURIS_EVT_SEGMENT_END)   ++segment_ends;
    }

    auris_stats_t stats{};
    if (auto s = auris_get_stats(engine, &stats); s != AURIS_OK) die("get_stats", s);

    if (auto s = auris_stop(engine); s != AURIS_OK) die("stop", s);

    // After stop, the worker has emitted SEGMENT_END. Drain events again.
    while (true) {
        auris_event_t evt{};
        const auris_status_t es = auris_poll_event(engine, &evt);
        if (es == AURIS_ERR_QUEUE_EMPTY) break;
        if (evt.type == AURIS_EVT_SEGMENT_END) ++segment_ends;
    }

    auris_destroy(engine);

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
    std::printf("smoke OK (auris %s)\n", auris_version_string());
    return 0;
}
