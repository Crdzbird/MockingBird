// tests/test_backend_faults.cpp
//
// Capture-backend fault injection via the make_backend() test seam. A mock
// CaptureBackend lets us drive paths that real devices can't be made to hit
// deterministically:
//   - init() failure  -> mockingbird_start returns the error, no worker spawned
//   - start() failure -> mockingbird_start tears the just-spawned worker back down
//     cleanly and returns the error (no thread leak, no hang)
//   - success + feed  -> the backend's own thread (T1) drives PCM through the
//     callback into the ring and out as packets (the path HOST_PCM bypasses)
//   - destroy while running -> teardown joins the worker AND stops the backend

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "capture/capture_backend.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {
struct MockState {
    mockingbird_status_t init_result  = MOCKINGBIRD_OK;
    mockingbird_status_t start_result = MOCKINGBIRD_OK;
    bool           feed         = false;
    int            init_calls   = 0;
    int            start_calls  = 0;
    int            stop_calls   = 0;
};
MockState g_mock;

class MockBackend final : public mockingbird::CaptureBackend {
public:
    ~MockBackend() override { stop(); }

    mockingbird_status_t init(const mockingbird_config_t& cfg,
                        mockingbird::PcmCallback     cb,
                        void*                  user) override {
        ++g_mock.init_calls;
        cb_ = cb;
        user_ = user;
        frame_samples_ = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
        return g_mock.init_result;
    }

    mockingbird_status_t start() override {
        ++g_mock.start_calls;
        if (g_mock.start_result != MOCKINGBIRD_OK) return g_mock.start_result;
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
        return MOCKINGBIRD_OK;
    }

    mockingbird_status_t stop() override {
        ++g_mock.stop_calls;
        running_.store(false, std::memory_order_relaxed);
        if (feeder_.joinable()) feeder_.join();
        return MOCKINGBIRD_OK;
    }

private:
    mockingbird::PcmCallback cb_   = nullptr;
    void*              user_ = nullptr;
    uint32_t           frame_samples_ = 320;
    std::atomic<bool>  running_{false};
    std::thread        feeder_;
};

std::unique_ptr<mockingbird::CaptureBackend> mock_factory(mockingbird_backend_t) {
    return std::make_unique<MockBackend>();
}

struct MockGuard {
    MockGuard() {
        g_mock = MockState{};
        mockingbird::set_backend_factory_override(&mock_factory);
    }
    ~MockGuard() { mockingbird::set_backend_factory_override(nullptr); }
};

mockingbird_config_t device_cfg() {
    mockingbird_config_t c{};
    mockingbird_config_defaults(&c);
    c.backend            = MOCKINGBIRD_BACKEND_MINIAUDIO;
    c.codec              = MOCKINGBIRD_CODEC_PCM_S16LE;
    c.sample_rate_hz     = 16000;
    c.frame_ms           = 20;
    c.enable_vad         = 0;
    c.enable_anomaly     = 0;
    c.enable_sound_class = 0;
    return c;
}

}

TEST_CASE("Backend fault: init() failure surfaces from mockingbird_start, no worker spawned") {
    MockGuard g;
    g_mock.init_result = MOCKINGBIRD_ERR_PERMISSION;

    mockingbird_config_t cfg = device_cfg();
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);

    CHECK(mockingbird_start(e) == MOCKINGBIRD_ERR_PERMISSION);
    CHECK(g_mock.init_calls == 1);
    CHECK(g_mock.start_calls == 0);
    CHECK(mockingbird_stop(e) == MOCKINGBIRD_ERR_NOT_RUNNING);

    mockingbird_destroy(e);
}

TEST_CASE("Backend fault: start() failure tears the worker back down cleanly") {
    MockGuard g;
    g_mock.start_result = MOCKINGBIRD_ERR_BACKEND;

    mockingbird_config_t cfg = device_cfg();
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);

    CHECK(mockingbird_start(e) == MOCKINGBIRD_ERR_BACKEND);
    CHECK(g_mock.init_calls == 1);
    CHECK(g_mock.start_calls == 1);
    CHECK(mockingbird_stop(e) == MOCKINGBIRD_ERR_NOT_RUNNING);

    g_mock.start_result = MOCKINGBIRD_OK;
    g_mock.feed = false;
    CHECK(mockingbird_start(e) == MOCKINGBIRD_OK);
    CHECK(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}

TEST_CASE("Backend success: T1 feed drives capture -> ring -> packets end to end") {
    MockGuard g;
    g_mock.feed = true;

    mockingbird_config_t cfg = device_cfg();
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);

    uint32_t packets = 0;
    for (int i = 0; i < 8; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        mockingbird_packet_t pkt{};
        while (mockingbird_drain(e, &pkt) == MOCKINGBIRD_OK) {
            ++packets;
            REQUIRE(mockingbird_release(e, &pkt) == MOCKINGBIRD_OK);
        }
    }

    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    CHECK(g_mock.stop_calls >= 1);
    mockingbird_destroy(e);

    INFO("captured=" << st.frames_captured << " emitted=" << st.packets_emitted
         << " drained=" << packets);
    CHECK(st.frames_captured > 0);
    CHECK(st.packets_emitted > 0);
}

TEST_CASE("Backend teardown: destroy while running stops the backend and joins") {
    MockGuard g;
    g_mock.feed = true;

    mockingbird_config_t cfg = device_cfg();
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    mockingbird_destroy(e);
    CHECK(g_mock.stop_calls >= 1);
}
