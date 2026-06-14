// tests/test_backend_faults.cpp
//
// Capture-backend fault injection via the make_backend() test seam. A mock
// CaptureBackend lets us drive paths that real devices can't be made to hit
// deterministically:
//   - init() failure  -> auris_start returns the error, no worker spawned
//   - start() failure -> auris_start tears the just-spawned worker back down
//     cleanly and returns the error (no thread leak, no hang)
//   - success + feed  -> the backend's own thread (T1) drives PCM through the
//     callback into the ring and out as packets (the path HOST_PCM bypasses)
//   - destroy while running -> teardown joins the worker AND stops the backend

#include "doctest.h"
#include "auris/auris.h"
#include "capture/capture_backend.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {

// Plain (copyable) knobs + counters. init/start/stop run on the host thread
// serially, so non-atomic counters are safe; only the feeder touches the
// backend's own atomic running flag.
struct MockState {
    auris_status_t init_result  = AURIS_OK;
    auris_status_t start_result = AURIS_OK;
    bool           feed         = false;
    int            init_calls   = 0;
    int            start_calls  = 0;
    int            stop_calls   = 0;
};
MockState g_mock;

class MockBackend final : public auris::CaptureBackend {
public:
    ~MockBackend() override { stop(); }

    auris_status_t init(const auris_config_t& cfg,
                        auris::PcmCallback     cb,
                        void*                  user) override {
        ++g_mock.init_calls;
        cb_ = cb;
        user_ = user;
        frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
        return g_mock.init_result;
    }

    auris_status_t start() override {
        ++g_mock.start_calls;
        if (g_mock.start_result != AURIS_OK) return g_mock.start_result;
        if (g_mock.feed) {
            running_.store(true, std::memory_order_relaxed);
            feeder_ = std::thread([this] {
                std::vector<int16_t> frame(frame_samples_, 1200);
                uint64_t ts = 0;
                while (running_.load(std::memory_order_relaxed)) {
                    if (cb_) cb_(user_, frame.data(), frame_samples_, ts);
                    ts += 20'000ull;
                    std::this_thread::sleep_for(std::chrono::microseconds(300));
                }
            });
        }
        return AURIS_OK;
    }

    auris_status_t stop() override {
        ++g_mock.stop_calls;
        running_.store(false, std::memory_order_relaxed);
        if (feeder_.joinable()) feeder_.join();
        return AURIS_OK;
    }

private:
    auris::PcmCallback cb_   = nullptr;
    void*              user_ = nullptr;
    uint32_t           frame_samples_ = 320;
    std::atomic<bool>  running_{false};
    std::thread        feeder_;
};

std::unique_ptr<auris::CaptureBackend> mock_factory(auris_backend_t) {
    return std::make_unique<MockBackend>();
}

// RAII: install the mock factory and reset state; restore the real factory on
// scope exit so other test files are unaffected.
struct MockGuard {
    MockGuard() {
        g_mock = MockState{};
        auris::set_backend_factory_override(&mock_factory);
    }
    ~MockGuard() { auris::set_backend_factory_override(nullptr); }
};

auris_config_t device_cfg() {
    auris_config_t c{};
    auris_config_defaults(&c);
    c.backend            = AURIS_BACKEND_MINIAUDIO;   // non-HOST_PCM -> make_backend
    c.codec              = AURIS_CODEC_PCM_S16LE;
    c.sample_rate_hz     = 16000;
    c.frame_ms           = 20;
    c.enable_vad         = 0;        // passthrough
    c.enable_anomaly     = 0;
    c.enable_sound_class = 0;
    return c;
}

} // namespace

TEST_CASE("Backend fault: init() failure surfaces from auris_start, no worker spawned") {
    MockGuard g;
    g_mock.init_result = AURIS_ERR_PERMISSION;

    auris_config_t cfg = device_cfg();
    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);     // create doesn't touch the device

    CHECK(auris_start(e) == AURIS_ERR_PERMISSION);   // init failure propagates
    CHECK(g_mock.init_calls == 1);
    CHECK(g_mock.start_calls == 0);                  // never reached start()
    CHECK(auris_stop(e) == AURIS_ERR_NOT_RUNNING);   // engine never ran

    auris_destroy(e);
}

TEST_CASE("Backend fault: start() failure tears the worker back down cleanly") {
    MockGuard g;
    g_mock.start_result = AURIS_ERR_BACKEND;

    auris_config_t cfg = device_cfg();
    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);

    // init() succeeds, the worker is spawned, then start() fails — auris_start
    // must join the worker and report the error without leaking the thread.
    CHECK(auris_start(e) == AURIS_ERR_BACKEND);
    CHECK(g_mock.init_calls == 1);
    CHECK(g_mock.start_calls == 1);
    CHECK(auris_stop(e) == AURIS_ERR_NOT_RUNNING);   // not left running

    // And it can be started again (proves no half-torn-down state) — clear the
    // failure first.
    g_mock.start_result = AURIS_OK;
    g_mock.feed = false;
    CHECK(auris_start(e) == AURIS_OK);
    CHECK(auris_stop(e) == AURIS_OK);
    auris_destroy(e);
}

TEST_CASE("Backend success: T1 feed drives capture -> ring -> packets end to end") {
    MockGuard g;
    g_mock.feed = true;

    auris_config_t cfg = device_cfg();
    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);
    REQUIRE(auris_start(e) == AURIS_OK);

    uint32_t packets = 0;
    for (int i = 0; i < 8; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        auris_packet_t pkt{};
        while (auris_drain(e, &pkt) == AURIS_OK) {
            ++packets;
            REQUIRE(auris_release(e, &pkt) == AURIS_OK);
        }
    }

    auris_stats_t st{};
    REQUIRE(auris_get_stats(e, &st) == AURIS_OK);
    REQUIRE(auris_stop(e) == AURIS_OK);
    CHECK(g_mock.stop_calls >= 1);                   // engine stopped the backend
    auris_destroy(e);

    INFO("captured=" << st.frames_captured << " emitted=" << st.packets_emitted
         << " drained=" << packets);
    CHECK(st.frames_captured > 0);                   // the T1 callback path worked
    CHECK(st.packets_emitted > 0);
}

TEST_CASE("Backend teardown: destroy while running stops the backend and joins") {
    MockGuard g;
    g_mock.feed = true;

    auris_config_t cfg = device_cfg();
    auris_engine_t* e = nullptr;
    REQUIRE(auris_create(&cfg, &e) == AURIS_OK);
    REQUIRE(auris_start(e) == AURIS_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // No explicit stop — destroy must stop the backend and join the worker.
    auris_destroy(e);
    CHECK(g_mock.stop_calls >= 1);
}
