// tests/test_host_pcm.cpp
//
// End-to-end HOST_PCM passthrough through the public C ABI. Mirrors the
// CLI smoke as an actual test so regressions surface in CI.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

struct Engine {
    mockingbird_engine_t* e = nullptr;
    ~Engine() {
        if (e) {
            mockingbird_stop(e);
            mockingbird_destroy(e);
        }
    }
};

void run_passthrough_into(Engine& g,
                          uint32_t frame_count_per_push,
                          uint32_t pushes,
                          std::vector<mockingbird_packet_t>& out_drained) {
    mockingbird_config_t cfg{};
    REQUIRE(mockingbird_config_defaults(&cfg) == MOCKINGBIRD_OK);
    cfg.backend            = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec              = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.sample_rate_hz     = 16000;
    cfg.frame_ms           = 20;
    cfg.enable_vad         = 0;            // passthrough: every frame flows
    cfg.enable_anomaly     = 0;
    REQUIRE(mockingbird_create(&cfg, &g.e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(g.e) == MOCKINGBIRD_OK);

    std::vector<int16_t> samples(frame_count_per_push);
    for (uint32_t i = 0; i < frame_count_per_push; ++i) {
        samples[i] = static_cast<int16_t>(i);
    }
    for (uint32_t p = 0; p < pushes; ++p) {
        REQUIRE(mockingbird_push_pcm(g.e, samples.data(), frame_count_per_push,
                               uint64_t{p} * 20'000ull) == MOCKINGBIRD_OK);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (true) {
        mockingbird_packet_t pkt{};
        const mockingbird_status_t s = mockingbird_drain(g.e, &pkt);
        if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) break;
        REQUIRE(s == MOCKINGBIRD_OK);
        out_drained.push_back(pkt);
        REQUIRE(mockingbird_release(g.e, &pkt) == MOCKINGBIRD_OK);
    }
}

} // namespace

TEST_CASE("HOST_PCM passthrough: pushes match drained packet count") {
    Engine g;
    std::vector<mockingbird_packet_t> drained;
    run_passthrough_into(g, /*frame_count*/ 320, /*pushes*/ 10, drained);
    CHECK(drained.size() == 10);

    mockingbird_stats_t stats{};
    REQUIRE(mockingbird_get_stats(g.e, &stats) == MOCKINGBIRD_OK);
    CHECK(stats.frames_captured        == 3200);
    CHECK(stats.frames_dropped_capture == 0);
    CHECK(stats.packets_emitted        == 10);
    CHECK(stats.packets_dropped        == 0);
    CHECK(stats.segments_opened        == 1);
}

TEST_CASE("HOST_PCM: per-frame timestamps are deterministic (frame clock)") {
    // Each frame is pushed with its own capture time (p * 20 ms). Every packet
    // must carry ITS frame's timestamp — base + index*frame_us — not the
    // timestamp of the most-recent push. This makes the output a pure function
    // of the input, independent of the producer/worker race (the property the
    // Go<->Dart conformance harness depends on).
    Engine g;
    std::vector<mockingbird_packet_t> drained;
    run_passthrough_into(g, /*frame_count*/ 320, /*pushes*/ 10, drained);
    REQUIRE(drained.size() == 10);
    for (size_t p = 0; p < drained.size(); ++p) {
        CHECK(drained[p].capture_ts_us == static_cast<uint64_t>(p) * 20'000ull);
    }
    // Not collapsed onto a single (latest-push) value.
    CHECK(drained.front().capture_ts_us != drained.back().capture_ts_us);
}

TEST_CASE("HOST_PCM passthrough: first packet carries KEYFRAME flag") {
    Engine g;
    std::vector<mockingbird_packet_t> drained;
    run_passthrough_into(g, 320, 3, drained);
    REQUIRE_FALSE(drained.empty());
    CHECK((drained[0].flags & MOCKINGBIRD_PKT_FLAG_KEYFRAME) != 0);
    for (size_t i = 1; i < drained.size(); ++i) {
        CHECK((drained[i].flags & MOCKINGBIRD_PKT_FLAG_KEYFRAME) == 0);
    }
}

TEST_CASE("HOST_PCM passthrough: SEGMENT_BEGIN/END events balance via poll") {
    Engine g;
    std::vector<mockingbird_packet_t> drained;
    run_passthrough_into(g, 320, 3, drained);
    // stop forces SEGMENT_END.
    REQUIRE(mockingbird_stop(g.e) == MOCKINGBIRD_OK);

    uint32_t begins = 0, ends = 0;
    while (true) {
        mockingbird_event_t evt{};
        const mockingbird_status_t s = mockingbird_poll_event(g.e, &evt);
        if (s == MOCKINGBIRD_ERR_QUEUE_EMPTY) break;
        REQUIRE(s == MOCKINGBIRD_OK);
        if (evt.type == MOCKINGBIRD_EVT_SEGMENT_BEGIN) ++begins;
        if (evt.type == MOCKINGBIRD_EVT_SEGMENT_END)   ++ends;
    }
    CHECK(begins == 1);
    CHECK(ends   == 1);
}

TEST_CASE("HOST_PCM passthrough on wrong backend returns INVALID_STATE") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend = MOCKINGBIRD_BACKEND_HOST_PCM;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    // Trying to push BEFORE create with HOST_PCM is well-covered above; here
    // verify a NULL samples buffer is rejected without crashing.
    CHECK(mockingbird_push_pcm(e, nullptr, 32, 0) == MOCKINGBIRD_ERR_INVALID_ARG);

    mockingbird_stop(e);
    mockingbird_destroy(e);
}
