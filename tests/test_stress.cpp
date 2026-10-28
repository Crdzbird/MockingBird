// tests/test_stress.cpp
//
// Lifecycle + concurrency hardening. Two flavours:
//   1. Repeated lifecycle churn: proves create/start/stop/destroy reclaim
//      everything (especially the worker thread). If stop() ever failed to
//      join, the next start()'s std::thread reassignment would std::terminate,
//      so a clean loop is a strong thread-leak check. Run under ASan/UBSan to
//      also catch use-after-free / heap corruption across the churn.
//   2. Concurrent producer + host-consumer under load: exercises the three
//      coupling points (SPSC PCM ring, bounded packet pool, event queue) from
//      separate threads, matching the documented model: ONE producer pushes,
//      ONE host thread drains/releases/polls, the worker runs internally. Run
//      under TSan to catch data races in the lock-free structures.

#include "doctest.h"
#include "mockingbird/mockingbird.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace {
mockingbird_config_t host_pcm_cfg(int vad, int anomaly) {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend             = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec               = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.sample_rate_hz      = 16000;
    cfg.frame_ms            = 20;
    cfg.enable_vad          = vad;
    cfg.enable_anomaly      = anomaly;
    cfg.enable_sound_class  = 0;
    cfg.pcm_ring_frames     = 1u << 14;
    cfg.packet_queue_capacity = 256;
    return cfg;
}

}

TEST_CASE("Stress: 150 full create/start/push/drain/stop/destroy cycles stay clean") {
    std::vector<int16_t> frame(320, 1200);
    for (int i = 0; i < 150; ++i) {
        mockingbird_config_t cfg = host_pcm_cfg(/*vad=*/i & 1, /*anomaly=*/!(i & 1));
        mockingbird_engine_t* e = nullptr;
        REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
        REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
        for (int k = 0; k < 8; ++k) {
            mockingbird_push_pcm(e, frame.data(), 320, static_cast<uint64_t>(k) * 20'000ull);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        mockingbird_packet_t pkt{};
        while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) mockingbird_release(e, &pkt);
        REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
        mockingbird_destroy(e);
    }
    CHECK(true);
}

TEST_CASE("Stress: repeated start/stop on ONE engine joins the worker every time") {
    mockingbird_config_t cfg = host_pcm_cfg(/*vad=*/1, /*anomaly=*/1);
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);

    std::vector<int16_t> frame(320, 800);
    for (int i = 0; i < 200; ++i) {
        REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
        mockingbird_push_pcm(e, frame.data(), 320, 0);
        REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    }
    mockingbird_destroy(e);
    CHECK(true);
}

TEST_CASE("Stress: concurrent producer + host-consumer under load is race-free") {
    mockingbird_config_t cfg = host_pcm_cfg(/*vad=*/1, /*anomaly=*/1);
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    std::atomic<bool>     stop{false};
    std::atomic<uint64_t> pushed{0};
    std::atomic<uint64_t> drained{0};

    std::thread producer([&] {
        std::vector<int16_t> frame(320);
        uint64_t ts = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            const int16_t v = static_cast<int16_t>(ts & 0x3FFF);
            for (auto& s : frame) s = v;
            if (mockingbird_push_pcm(e, frame.data(), 320, ts) == MOCKINGBIRD_OK) {
                pushed.fetch_add(1, std::memory_order_relaxed);
            }
            ts += 20'000ull;
        }
    });

    std::thread consumer([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            mockingbird_packet_t pkt{};
            while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) {
                drained.fetch_add(1, std::memory_order_relaxed);
                mockingbird_release(e, &pkt);
            }
            mockingbird_event_t evt{};
            while (mockingbird_poll_event(e, &evt) == MOCKINGBIRD_OK) { /* discard */ }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    stop.store(true, std::memory_order_relaxed);
    producer.join();
    consumer.join();

    mockingbird_packet_t pkt{};
    while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) {
        mockingbird_release(e, &pkt);
        drained.fetch_add(1, std::memory_order_relaxed);
    }

    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);

    INFO("pushed=" << pushed.load() << " drained=" << drained.load()
         << " captured=" << st.frames_captured
         << " emitted=" << st.packets_emitted
         << " dropped_cap=" << st.frames_dropped_capture);
    CHECK(pushed.load() > 0);
    CHECK(st.frames_captured > 0);
    CHECK(drained.load() <= st.packets_emitted);
}

TEST_CASE("Soak: sustained pipeline keeps making forward progress, no stall") {
    mockingbird_config_t cfg = host_pcm_cfg(/*vad=*/1, /*anomaly=*/1);
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    std::atomic<bool> stop{false};
    std::thread producer([&] {
        std::vector<int16_t> frame(320);
        uint64_t ts = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            const int16_t v = static_cast<int16_t>((ts / 20'000ull) & 0x1FFF);
            for (auto& s : frame) s = v;
            mockingbird_push_pcm(e, frame.data(), 320, ts);
            ts += 20'000ull;
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });
    std::thread consumer([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            mockingbird_packet_t pkt{};
            while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) mockingbird_release(e, &pkt);
            mockingbird_event_t evt{};
            while (mockingbird_poll_event(e, &evt) == MOCKINGBIRD_OK) { /* discard */ }
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });

    uint64_t last_captured = 0, last_emitted = 0;
    int progressing_windows = 0;
    for (int w = 0; w < 6; ++w) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        mockingbird_stats_t st{};
        REQUIRE(mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK);
        CHECK(st.frames_captured >= last_captured);
        CHECK(st.packets_emitted >= last_emitted);
        if (st.frames_captured > last_captured) ++progressing_windows;
        last_captured = st.frames_captured;
        last_emitted  = st.packets_emitted;
    }

    stop.store(true, std::memory_order_relaxed);
    producer.join();
    consumer.join();
    mockingbird_packet_t pkt{};
    while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) mockingbird_release(e, &pkt);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);

    INFO("progressing_windows=" << progressing_windows << "/6 captured=" << last_captured);
    CHECK(progressing_windows >= 5);
    CHECK(last_captured > 10000);
}
