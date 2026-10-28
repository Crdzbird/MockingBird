// tests/test_capture_notices.cpp
//
// What a capture backend reports reaches the host: interruption, resume and
// route events arrive on the event queue, restarts are counted, the applied
// voice processing shows in the stats, and the device a host selects is the
// one the backend is asked to open.

#include "doctest.h"
#include "mockingbird/mockingbird.h"
#include "capture/capture_backend.hpp"

#include <string>
#include <vector>

namespace {
struct NoticeState {
    mockingbird::CaptureNoticeCallback notice = nullptr;
    void*                              user   = nullptr;
    std::string                        selected = "unset";
    uint32_t                           voice    = 0;
};
NoticeState g_state;

class NoticeBackend final : public mockingbird::CaptureBackend {
public:
    mockingbird_status_t init(const mockingbird_config_t& cfg, mockingbird::PcmCallback,
                              void*) override {
        g_state.voice = cfg.voice_processing & MOCKINGBIRD_VOICE_ECHO_CANCEL;
        return MOCKINGBIRD_OK;
    }
    mockingbird_status_t start() override { return MOCKINGBIRD_OK; }
    mockingbird_status_t stop() override { return MOCKINGBIRD_OK; }
    void set_notice_callback(mockingbird::CaptureNoticeCallback cb, void* user) override {
        g_state.notice = cb;
        g_state.user   = user;
    }
    mockingbird_status_t select_device(const std::string& id) override {
        g_state.selected = id;
        return MOCKINGBIRD_OK;
    }
    uint32_t voice_processing_active() const override { return g_state.voice; }
};

std::unique_ptr<mockingbird::CaptureBackend> notice_factory(mockingbird_backend_t) {
    return std::make_unique<NoticeBackend>();
}

struct Guard {
    Guard() {
        g_state = NoticeState{};
        mockingbird::set_backend_factory_override(&notice_factory);
    }
    ~Guard() { mockingbird::set_backend_factory_override(nullptr); }
};

mockingbird_engine_t* capture_engine(uint32_t voice) {
    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend          = MOCKINGBIRD_BACKEND_MINIAUDIO;
    cfg.enable_vad       = 0;
    cfg.enable_anomaly   = 0;
    cfg.voice_processing = voice;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    return e;
}

std::vector<int> drain_types(mockingbird_engine_t* e) {
    std::vector<int> types;
    mockingbird_event_t evt{};
    while (mockingbird_poll_event(e, &evt) == MOCKINGBIRD_OK) types.push_back(static_cast<int>(evt.type));
    return types;
}

}

TEST_CASE("backend notices become events and restarts are counted") {
    Guard g;
    mockingbird_engine_t* e = capture_engine(0);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    REQUIRE(g_state.notice != nullptr);
    g_state.notice(g_state.user, MOCKINGBIRD_EVT_CAPTURE_INTERRUPTED, 0);
    g_state.notice(g_state.user, MOCKINGBIRD_EVT_CAPTURE_RESUMED, 0);
    g_state.notice(g_state.user, MOCKINGBIRD_EVT_ROUTE_CHANGED, 0);
    CHECK(drain_types(e) == std::vector<int>{MOCKINGBIRD_EVT_CAPTURE_INTERRUPTED,
                                             MOCKINGBIRD_EVT_CAPTURE_RESUMED,
                                             MOCKINGBIRD_EVT_ROUTE_CHANGED});
    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK);
    CHECK(st.capture_restarts == 2u);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}

TEST_CASE("the applied voice processing shows in the stats") {
    Guard g;
    mockingbird_engine_t* e = capture_engine(MOCKINGBIRD_VOICE_ECHO_CANCEL | MOCKINGBIRD_VOICE_GAIN_CONTROL);
    mockingbird_stats_t st{};
    REQUIRE(mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK);
    CHECK(st.voice_processing_active == 0u);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_get_stats(e, &st) == MOCKINGBIRD_OK);
    CHECK(st.voice_processing_active == MOCKINGBIRD_VOICE_ECHO_CANCEL);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    mockingbird_destroy(e);
}

TEST_CASE("the selected input device is the one the backend opens") {
    Guard g;
    mockingbird_engine_t* e = capture_engine(0);
    REQUIRE(mockingbird_set_input_device(e, "usb-mic") == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    CHECK(g_state.selected == "usb-mic");
    CHECK(mockingbird_set_input_device(e, nullptr) == MOCKINGBIRD_ERR_INVALID_STATE);
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_set_input_device(e, nullptr) == MOCKINGBIRD_OK);
    REQUIRE(mockingbird_start(e) == MOCKINGBIRD_OK);
    CHECK(g_state.selected.empty());
    REQUIRE(mockingbird_stop(e) == MOCKINGBIRD_OK);
    CHECK(mockingbird_set_input_device(e, std::string(300, 'x').c_str()) == MOCKINGBIRD_ERR_INVALID_ARG);
    mockingbird_destroy(e);
}

TEST_CASE("device calls validate their arguments") {
    uint32_t n = 7;
    CHECK(mockingbird_list_input_devices(MOCKINGBIRD_BACKEND_HOST_PCM, nullptr, 0, &n) ==
          MOCKINGBIRD_ERR_UNSUPPORTED);
    CHECK(n == 0u);
    CHECK(mockingbird_list_input_devices(MOCKINGBIRD_BACKEND_MINIAUDIO, nullptr, 4, &n) ==
          MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_list_input_devices(MOCKINGBIRD_BACKEND_MINIAUDIO, nullptr, 0, nullptr) ==
          MOCKINGBIRD_ERR_INVALID_ARG);
    CHECK(mockingbird_set_input_device(nullptr, "x") == MOCKINGBIRD_ERR_INVALID_ARG);

    mockingbird_config_t cfg{};
    mockingbird_config_defaults(&cfg);
    cfg.backend = MOCKINGBIRD_BACKEND_HOST_PCM;
    mockingbird_engine_t* e = nullptr;
    REQUIRE(mockingbird_create(&cfg, &e) == MOCKINGBIRD_OK);
    CHECK(mockingbird_set_input_device(e, "x") == MOCKINGBIRD_ERR_UNSUPPORTED);
    mockingbird_destroy(e);
}
