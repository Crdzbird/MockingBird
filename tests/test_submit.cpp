// tests/test_submit.cpp
//
// mockingbird_submit and mockingbird_player_submit: the call returns while a
// slow output opens on the lifecycle thread, queued calls run in order with
// one done each, packets pushed before the start finishes play once it does,
// and destroy runs what is still queued. Meant to run under ThreadSanitizer.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "play/output_backend.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
constexpr auto kOpen = std::chrono::milliseconds(300);
std::atomic<int> g_outputs_alive{0};
std::atomic<int> g_outputs_made{0};

class SlowOutput final : public mockingbird::OutputBackend {
public:
    SlowOutput() {
        g_outputs_alive.fetch_add(1);
        g_outputs_made.fetch_add(1);
    }

    ~SlowOutput() override {
        stop();
        g_outputs_alive.fetch_sub(1);
    }

    mockingbird_status_t init(const mockingbird::OutputFormat& fmt, mockingbird::RenderCallback cb, void* user,
                              mockingbird::OutputNoticeCallback, void*) override {
        cb_     = cb;
        user_   = user;
        period_ = fmt.period_frames;
        buf_.assign(size_t{period_} * fmt.channels, 0);
        return MOCKINGBIRD_OK;
    }

    mockingbird_status_t start() override {
        if (thread_.joinable()) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
        std::this_thread::sleep_for(kOpen);
        run_.store(true);
        thread_ = std::thread([this] {
            while (run_.load()) {
                cb_(user_, buf_.data(), period_);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
        return MOCKINGBIRD_OK;
    }

    mockingbird_status_t stop() override {
        run_.store(false);
        if (thread_.joinable()) thread_.join();
        return MOCKINGBIRD_OK;
    }

private:
    mockingbird::RenderCallback cb_     = nullptr;
    void*                       user_   = nullptr;
    uint32_t                    period_ = 0;
    std::vector<int16_t>        buf_;
    std::atomic<bool>           run_{false};
    std::thread                 thread_;
};

std::unique_ptr<mockingbird::OutputBackend> slow_factory(mockingbird_backend_t) {
    return std::make_unique<SlowOutput>();
}

struct Done {
    std::mutex                                mu;
    std::vector<std::pair<uint32_t, int32_t>> calls;

    static void record(void* user, uint32_t op, int32_t status) {
        auto* d = static_cast<Done*>(user);
        std::lock_guard lk(d->mu);
        d->calls.emplace_back(op, status);
    }

    size_t size() {
        std::lock_guard lk(mu);
        return calls.size();
    }

    void wait_for(size_t n) {
        const auto until = Clock::now() + std::chrono::seconds(5);
        while (size() < n && Clock::now() < until) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        REQUIRE(size() == n);
    }
};

mockingbird_player_t* make_slow_player() {
    mockingbird::set_output_factory_override(&slow_factory);
    mockingbird_player_config_t c{};
    mockingbird_player_config_defaults(&c);
    c.target_delay_ms = 20;
    mockingbird_player_t* p = nullptr;
    REQUIRE(mockingbird_player_create(&c, &p) == MOCKINGBIRD_OK);
    return p;
}

mockingbird_player_stats_t stats(mockingbird_player_t* p) {
    mockingbird_player_stats_t s{};
    mockingbird_player_get_stats(p, &s);
    return s;
}

}

TEST_CASE("player submit: start returns before a slow output opens, and early packets play") {
    mockingbird_player_t* p = make_slow_player();
    Done done;
    const auto t0 = Clock::now();
    REQUIRE(mockingbird_player_submit(p, MOCKINGBIRD_PLAYER_OP_START, &Done::record, &done) == MOCKINGBIRD_OK);
    CHECK(Clock::now() - t0 < kOpen / 3);
    std::vector<uint8_t> pcm(640, 1);
    for (uint64_t i = 0; i < 5; ++i) {
        REQUIRE(mockingbird_player_push(p, MOCKINGBIRD_CODEC_PCM_S16LE, pcm.data(), 640, i * 20000, 0) ==
                MOCKINGBIRD_OK);
    }
    CHECK(done.size() == 0);
    done.wait_for(1);
    CHECK(done.calls[0] == std::make_pair(uint32_t{MOCKINGBIRD_PLAYER_OP_START}, int32_t{MOCKINGBIRD_OK}));
    const auto until = Clock::now() + std::chrono::seconds(2);
    while (stats(p).packets_played < 5 && Clock::now() < until) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    CHECK(stats(p).packets_received == 5);
    CHECK(stats(p).packets_played == 5);
    CHECK(stats(p).packets_dropped == 0);
    mockingbird_player_destroy(p);
    mockingbird::set_output_factory_override(nullptr);
}

TEST_CASE("player submit: queued calls run in order with one done each") {
    mockingbird_player_t* p = make_slow_player();
    Done done;
    const mockingbird_player_op_t ops[] = {MOCKINGBIRD_PLAYER_OP_START, MOCKINGBIRD_PLAYER_OP_START,
                                           MOCKINGBIRD_PLAYER_OP_PAUSE, MOCKINGBIRD_PLAYER_OP_RESUME,
                                           MOCKINGBIRD_PLAYER_OP_STOP,  MOCKINGBIRD_PLAYER_OP_PAUSE};
    for (auto op : ops) REQUIRE(mockingbird_player_submit(p, op, &Done::record, &done) == MOCKINGBIRD_OK);
    done.wait_for(6);
    const std::vector<std::pair<uint32_t, int32_t>> want = {
        {MOCKINGBIRD_PLAYER_OP_START, MOCKINGBIRD_OK},  {MOCKINGBIRD_PLAYER_OP_START, MOCKINGBIRD_ERR_ALREADY_RUNNING},
        {MOCKINGBIRD_PLAYER_OP_PAUSE, MOCKINGBIRD_OK},  {MOCKINGBIRD_PLAYER_OP_RESUME, MOCKINGBIRD_OK},
        {MOCKINGBIRD_PLAYER_OP_STOP, MOCKINGBIRD_OK},   {MOCKINGBIRD_PLAYER_OP_PAUSE, MOCKINGBIRD_ERR_NOT_RUNNING}};
    CHECK(done.calls == want);
    CHECK(stats(p).state == MOCKINGBIRD_PLAYER_STOPPED);
    mockingbird_player_destroy(p);
    mockingbird::set_output_factory_override(nullptr);
}

TEST_CASE("player submit: a stop racing a start leaves the player stopped, and destroy runs what is queued") {
    mockingbird_player_t* p = make_slow_player();
    Done done;
    REQUIRE(mockingbird_player_submit(p, MOCKINGBIRD_PLAYER_OP_START, &Done::record, &done) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_player_submit(p, MOCKINGBIRD_PLAYER_OP_STOP, &Done::record, &done) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_player_submit(p, MOCKINGBIRD_PLAYER_OP_START, &Done::record, &done) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_player_submit(p, MOCKINGBIRD_PLAYER_OP_STOP, nullptr, nullptr) == MOCKINGBIRD_OK);
    mockingbird_player_destroy(p);
    REQUIRE(done.calls.size() == 3);
    CHECK(done.calls[1].second == MOCKINGBIRD_OK);
    CHECK(done.calls[2].second == MOCKINGBIRD_OK);
    mockingbird::set_output_factory_override(nullptr);
}

TEST_CASE("player submit: bad arguments queue nothing") {
    mockingbird_player_t* p = make_slow_player();
    CHECK(mockingbird_player_submit(nullptr, MOCKINGBIRD_PLAYER_OP_START, nullptr, nullptr) ==
          MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_submit(p, static_cast<mockingbird_player_op_t>(5), nullptr, nullptr) ==
          MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_player_submit(p, static_cast<mockingbird_player_op_t>(-1), nullptr, nullptr) ==
          MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(stats(p).state == MOCKINGBIRD_PLAYER_STOPPED);
    mockingbird_player_destroy(p);
    mockingbird::set_output_factory_override(nullptr);
}

TEST_CASE("engine submit: start and stop run in order off the caller's thread") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend        = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec          = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.enable_vad     = 0;
    cfg.enable_anomaly = 0;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    Done done;
    REQUIRE(mockingbird_submit(e, MOCKINGBIRD_ENGINE_OP_START, &Done::record, &done) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_submit(e, MOCKINGBIRD_ENGINE_OP_START, &Done::record, &done) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_submit(e, MOCKINGBIRD_ENGINE_OP_STOP, &Done::record, &done) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_submit(e, MOCKINGBIRD_ENGINE_OP_STOP, &Done::record, &done) == MOCKINGBIRD_OK);
    done.wait_for(4);
    const std::vector<std::pair<uint32_t, int32_t>> want = {
        {MOCKINGBIRD_ENGINE_OP_START, MOCKINGBIRD_OK},
        {MOCKINGBIRD_ENGINE_OP_START, MOCKINGBIRD_ERR_ALREADY_RUNNING},
        {MOCKINGBIRD_ENGINE_OP_STOP, MOCKINGBIRD_OK},
        {MOCKINGBIRD_ENGINE_OP_STOP, MOCKINGBIRD_ERR_NOT_RUNNING}};
    CHECK(done.calls == want);
    CHECK(mockingbird_submit(e, static_cast<mockingbird_engine_op_t>(3), nullptr, nullptr) ==
          MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_submit(nullptr, MOCKINGBIRD_ENGINE_OP_START, nullptr, nullptr) == MOCKINGBIRD_ERR_INVALID_ARG);
    REQUIRE(mockingbird_submit(e, MOCKINGBIRD_ENGINE_OP_START, &Done::record, &done) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
    CHECK(done.calls.size() == 5);
}

TEST_CASE("player submit: close releases the output, and a later start opens a new one") {
    mockingbird_player_t* p = make_slow_player();
    const int made = g_outputs_made.load();
    Done done;
    for (auto op : {MOCKINGBIRD_PLAYER_OP_START, MOCKINGBIRD_PLAYER_OP_CLOSE, MOCKINGBIRD_PLAYER_OP_CLOSE}) {
        REQUIRE(mockingbird_player_submit(p, op, &Done::record, &done) == MOCKINGBIRD_OK);
    }
    done.wait_for(3);
    CHECK(done.calls[1] == std::make_pair(uint32_t{MOCKINGBIRD_PLAYER_OP_CLOSE}, int32_t{MOCKINGBIRD_OK}));
    CHECK(done.calls[2] == std::make_pair(uint32_t{MOCKINGBIRD_PLAYER_OP_CLOSE}, int32_t{MOCKINGBIRD_OK}));
    CHECK(g_outputs_alive.load() == 0);
    CHECK(stats(p).state == MOCKINGBIRD_PLAYER_STOPPED);
    REQUIRE(mockingbird_player_submit(p, MOCKINGBIRD_PLAYER_OP_START, &Done::record, &done) == MOCKINGBIRD_OK);
    done.wait_for(4);
    CHECK(done.calls[3].second == MOCKINGBIRD_OK);
    CHECK(g_outputs_made.load() == made + 2);
    CHECK(g_outputs_alive.load() == 1);
    mockingbird_player_destroy(p);
    CHECK(g_outputs_alive.load() == 0);
    mockingbird::set_output_factory_override(nullptr);
}

TEST_CASE("engine submit: close stops a running engine and it can start again") {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend        = MOCKINGBIRD_BACKEND_HOST_PCM;
    cfg.codec          = MOCKINGBIRD_CODEC_PCM_S16LE;
    cfg.enable_vad     = 0;
    cfg.enable_anomaly = 0;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    Done done;
    for (auto op : {MOCKINGBIRD_ENGINE_OP_START, MOCKINGBIRD_ENGINE_OP_CLOSE, MOCKINGBIRD_ENGINE_OP_CLOSE,
                    MOCKINGBIRD_ENGINE_OP_START, MOCKINGBIRD_ENGINE_OP_STOP}) {
        REQUIRE(mockingbird_submit(e, op, &Done::record, &done) == MOCKINGBIRD_OK);
    }
    done.wait_for(5);
    for (const auto& call : done.calls) CHECK(call.second == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}
