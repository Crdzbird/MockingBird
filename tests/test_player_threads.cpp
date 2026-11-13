// tests/test_player_threads.cpp
//
// The player with a synthetic output that renders on its own thread, the way
// an OS audio thread does: start, pause, resume and stop race a pusher, a
// stats reader and volume changes; route and interruption notices arrive
// from yet another thread. Meant to run under ThreadSanitizer.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "play/output_backend.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {
std::atomic<mockingbird::OutputNoticeCallback> g_notice{nullptr};
std::atomic<void*>                             g_notice_user{nullptr};
std::atomic<uint64_t>                          g_rendered{0};

class FakeOutput final : public mockingbird::OutputBackend {
public:
    ~FakeOutput() override { stop(); }

    mockingbird_status_t init(const mockingbird::OutputFormat& fmt, mockingbird::RenderCallback cb, void* user,
                              mockingbird::OutputNoticeCallback notice, void* notice_user) override {
        cb_   = cb;
        user_ = user;
        buf_.assign(size_t{fmt.period_frames} * fmt.channels, 0);
        period_ = fmt.period_frames;
        g_notice.store(notice);
        g_notice_user.store(notice_user);
        return MOCKINGBIRD_OK;
    }

    mockingbird_status_t start() override {
        if (thread_.joinable()) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
        run_.store(true);
        thread_ = std::thread([this] {
            while (run_.load()) {
                cb_(user_, buf_.data(), period_);
                g_rendered.fetch_add(period_);
                std::this_thread::sleep_for(std::chrono::microseconds(500));
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
    mockingbird::RenderCallback cb_   = nullptr;
    void*                       user_ = nullptr;
    std::vector<int16_t>        buf_;
    uint32_t                    period_ = 0;
    std::atomic<bool>           run_{false};
    std::thread                 thread_;
};

std::unique_ptr<mockingbird::OutputBackend> fake_factory(mockingbird_backend_t) {
    return std::make_unique<FakeOutput>();
}

mockingbird_player_t* make_fake_player() {
    mockingbird::set_output_factory_override(&fake_factory);
    mockingbird_player_config_t c{};
    mockingbird_player_config_defaults(&c);
    c.target_delay_ms = 20;
    mockingbird_player_t* p = nullptr;
    REQUIRE(mockingbird_player_create(&c, &p) == MOCKINGBIRD_OK);
    return p;
}

void tell(mockingbird::OutputNotice n) { g_notice.load()(g_notice_user.load(), n); }

}

TEST_CASE("player threads: start, pause, resume and stop race a pusher and a stats reader") {
    mockingbird_player_t* p = make_fake_player();
    std::atomic<bool> done{false};
    std::thread pusher([&] {
        std::vector<uint8_t> pcm(640, 1);
        uint64_t pts = 0;
        while (!done.load()) {
            mockingbird_player_push(p, MOCKINGBIRD_CODEC_PCM_S16LE, pcm.data(), 640, pts, 0);
            pts += 20000;
            if (pts % 2'000'000 == 0) mockingbird_player_flush(p);
            std::this_thread::sleep_for(std::chrono::microseconds(300));
        }
    });
    std::thread reader([&] {
        mockingbird_player_stats_t s{};
        float v = 0.0f;
        while (!done.load()) {
            mockingbird_player_get_stats(p, &s);
            mockingbird_player_set_volume(p, v);
            mockingbird_player_set_muted(p, s.state == MOCKINGBIRD_PLAYER_PAUSED);
            v = v >= 1.0f ? 0.0f : v + 0.125f;
        }
    });
    for (int i = 0; i < 150; ++i) {
        REQUIRE(mockingbird_player_start(p) == MOCKINGBIRD_OK);
        std::this_thread::sleep_for(std::chrono::microseconds(800));
        REQUIRE(mockingbird_player_pause(p) == MOCKINGBIRD_OK);
        REQUIRE(mockingbird_player_resume(p) == MOCKINGBIRD_OK);
        REQUIRE(mockingbird_player_stop(p) == MOCKINGBIRD_OK);
    }
    done.store(true);
    pusher.join();
    reader.join();
    CHECK(g_rendered.load() > 0);
    const auto st = [&] {
        mockingbird_player_stats_t s{};
        mockingbird_player_get_stats(p, &s);
        return s;
    }();
    CHECK(st.state == MOCKINGBIRD_PLAYER_STOPPED);
    CHECK(st.packets_received > 0);
    mockingbird_player_destroy(p);
    mockingbird::set_output_factory_override(nullptr);
}

TEST_CASE("player threads: route and interruption notices from another thread set the state") {
    mockingbird_player_t* p = make_fake_player();
    REQUIRE(mockingbird_player_start(p) == MOCKINGBIRD_OK);
    mockingbird_player_stats_t s{};
    std::thread os([] {
        tell(mockingbird::OutputNotice::Interrupted);
        tell(mockingbird::OutputNotice::RouteChanged);
    });
    os.join();
    mockingbird_player_get_stats(p, &s);
    CHECK(s.state == MOCKINGBIRD_PLAYER_INTERRUPTED);
    CHECK(s.output_restarts == 1);
    std::thread back([] { tell(mockingbird::OutputNotice::Resumed); });
    back.join();
    std::thread lost([] { tell(mockingbird::OutputNotice::RouteLost); });
    lost.join();
    mockingbird_player_get_stats(p, &s);
    CHECK(s.state == MOCKINGBIRD_PLAYER_PAUSED);
    REQUIRE(mockingbird_player_resume(p) == MOCKINGBIRD_OK);
    mockingbird_player_get_stats(p, &s);
    CHECK(s.state != MOCKINGBIRD_PLAYER_PAUSED);
    CHECK(s.state != MOCKINGBIRD_PLAYER_INTERRUPTED);
    mockingbird_player_destroy(p);
    mockingbird::set_output_factory_override(nullptr);
}
