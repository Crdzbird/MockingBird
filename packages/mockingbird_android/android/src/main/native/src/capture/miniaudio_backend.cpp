// miniaudio capture backend. The library implementation is compiled once in
// miniaudio_impl.c; this unit only uses the API. A selected device is found
// by enumerating the backend's own context, and miniaudio's notifications
// become the engine's interruption and route events.
#include "capture/miniaudio_backend.hpp"

#include "capture/chunk_timestamp.hpp"
#include "capture/miniaudio_ids.hpp"

#include <atomic>
#include <cstdint>

namespace mockingbird {
namespace {
mockingbird_status_t map_ma_result(ma_result r) noexcept {
    switch (r) {
        case MA_SUCCESS:                return MOCKINGBIRD_OK;
        case MA_ACCESS_DENIED:          return MOCKINGBIRD_ERR_PERMISSION;
        case MA_NO_DEVICE:              return MOCKINGBIRD_ERR_BACKEND;
        case MA_DEVICE_NOT_INITIALIZED: return MOCKINGBIRD_ERR_INVALID_STATE;
        case MA_INVALID_ARGS:           return MOCKINGBIRD_ERR_INVALID_ARG;
        case MA_OUT_OF_MEMORY:          return MOCKINGBIRD_ERR_NO_MEMORY;
        default:                        return MOCKINGBIRD_ERR_BACKEND;
    }
}

}

struct MiniaudioBackend::Impl {
    ma_context            context{};
    ma_device             device{};
    bool                  context_initialized = false;
    bool                  device_initialized  = false;
    bool                  running             = false;
    std::atomic<bool>     stopping{false};
    PcmCallback           cb          = nullptr;
    void*                 user        = nullptr;
    CaptureNoticeCallback notice      = nullptr;
    void*                 notice_user = nullptr;
    uint32_t              sample_rate = 0;
    std::string           device_id;

    void tell(mockingbird_event_type_t type) {
        if (notice != nullptr) notice(notice_user, type, 0);
    }
};

static void miniaudio_data_callback(ma_device* device, void*, const void* input,
                                    ma_uint32 frame_count) {
    auto* impl = static_cast<MiniaudioBackend::Impl*>(device->pUserData);
    if (impl == nullptr || impl->cb == nullptr || input == nullptr || frame_count == 0) return;
    impl->cb(impl->user, static_cast<const int16_t*>(input), frame_count,
             chunk_start_ts_us(impl->sample_rate, frame_count));
}

static void miniaudio_notification(const ma_device_notification* n) {
    auto* impl = static_cast<MiniaudioBackend::Impl*>(n->pDevice->pUserData);
    if (impl == nullptr) return;
    switch (n->type) {
        case ma_device_notification_type_rerouted:
            impl->tell(MOCKINGBIRD_EVT_ROUTE_CHANGED);
            break;
        case ma_device_notification_type_interruption_began:
            impl->tell(MOCKINGBIRD_EVT_CAPTURE_INTERRUPTED);
            break;
        case ma_device_notification_type_interruption_ended:
            impl->tell(MOCKINGBIRD_EVT_CAPTURE_RESUMED);
            break;
        case ma_device_notification_type_stopped:
            if (!impl->stopping.load(std::memory_order_acquire)) {
                impl->tell(MOCKINGBIRD_EVT_CAPTURE_INTERRUPTED);
            }
            break;
        default:
            break;
    }
}

MiniaudioBackend::MiniaudioBackend() : impl_(std::make_unique<Impl>()) {}

MiniaudioBackend::~MiniaudioBackend() {
    impl_->stopping.store(true, std::memory_order_release);
    if (impl_->running) ma_device_stop(&impl_->device);
    if (impl_->device_initialized) ma_device_uninit(&impl_->device);
    if (impl_->context_initialized) ma_context_uninit(&impl_->context);
}

void MiniaudioBackend::set_notice_callback(CaptureNoticeCallback cb, void* user) {
    impl_->notice      = cb;
    impl_->notice_user = user;
}

mockingbird_status_t MiniaudioBackend::select_device(const std::string& id) {
    if (impl_->device_initialized) return MOCKINGBIRD_ERR_INVALID_STATE;
    impl_->device_id = id;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t MiniaudioBackend::init(const mockingbird_config_t& cfg, PcmCallback cb, void* user) {
    if (impl_->device_initialized) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (cb == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;

    impl_->cb          = cb;
    impl_->user        = user;
    impl_->sample_rate = cfg.sample_rate_hz;

    const ma_result c = ma_context_init(nullptr, 0, nullptr, &impl_->context);
    if (c != MA_SUCCESS) return map_ma_result(c);
    impl_->context_initialized = true;

    ma_device_id chosen{};
    ma_device_config dc   = ma_device_config_init(ma_device_type_capture);
    if (!impl_->device_id.empty()) {
        if (!miniaudio_find_capture_device(impl_->context, impl_->device_id, chosen)) {
            return MOCKINGBIRD_ERR_INVALID_ARG;
        }
        dc.capture.pDeviceID = &chosen;
    }
    dc.capture.format       = ma_format_s16;
    dc.capture.channels     = cfg.channels;
    dc.sampleRate           = cfg.sample_rate_hz;
    dc.periodSizeInFrames   = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
    dc.dataCallback         = miniaudio_data_callback;
    dc.notificationCallback = miniaudio_notification;
    dc.pUserData            = impl_.get();

    const ma_result r = ma_device_init(&impl_->context, &dc, &impl_->device);
    if (r != MA_SUCCESS) return map_ma_result(r);
    impl_->device_initialized = true;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t MiniaudioBackend::start() {
    if (!impl_->device_initialized) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (impl_->running) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
    impl_->stopping.store(false, std::memory_order_release);
    const ma_result r = ma_device_start(&impl_->device);
    if (r != MA_SUCCESS) return map_ma_result(r);
    impl_->running = true;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t MiniaudioBackend::stop() {
    if (!impl_->running) return MOCKINGBIRD_OK;
    impl_->stopping.store(true, std::memory_order_release);
    ma_device_stop(&impl_->device);
    impl_->running = false;
    return MOCKINGBIRD_OK;
}

}
