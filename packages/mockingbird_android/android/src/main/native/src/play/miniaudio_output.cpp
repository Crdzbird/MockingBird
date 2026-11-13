// miniaudio playback backend. The data callback runs on miniaudio's audio
// thread and only calls the player's renderer; notifications become output
// notices, and a stop the player did not ask for counts as an interruption.
#ifndef __EMSCRIPTEN__

#include "play/miniaudio_output.hpp"

#include "miniaudio.h"

#include <atomic>

namespace mockingbird {
struct MiniaudioOutput::Impl {
    ma_device            device{};
    bool                 initialized = false;
    bool                 running     = false;
    std::atomic<bool>    stopping{false};
    RenderCallback       cb          = nullptr;
    void*                user        = nullptr;
    OutputNoticeCallback notice      = nullptr;
    void*                notice_user = nullptr;

    void tell(OutputNotice n) const {
        if (notice != nullptr) notice(notice_user, n);
    }
};

namespace {
void data_callback(ma_device* device, void* output, const void*, ma_uint32 frames) {
    auto* impl = static_cast<MiniaudioOutput::Impl*>(device->pUserData);
    if (impl == nullptr || output == nullptr || frames == 0) return;
    impl->cb(impl->user, static_cast<int16_t*>(output), frames);
}

void notification(const ma_device_notification* n) {
    auto* impl = static_cast<MiniaudioOutput::Impl*>(n->pDevice->pUserData);
    if (impl == nullptr) return;
    switch (n->type) {
        case ma_device_notification_type_rerouted:           impl->tell(OutputNotice::RouteChanged); break;
        case ma_device_notification_type_interruption_began: impl->tell(OutputNotice::Interrupted); break;
        case ma_device_notification_type_interruption_ended: impl->tell(OutputNotice::Resumed); break;
        case ma_device_notification_type_stopped:
            if (!impl->stopping.load(std::memory_order_acquire)) impl->tell(OutputNotice::Interrupted);
            break;
        default: break;
    }
}

mockingbird_status_t map_result(ma_result r) {
    if (r == MA_SUCCESS) return MOCKINGBIRD_OK;
    if (r == MA_OUT_OF_MEMORY) return MOCKINGBIRD_ERR_NO_MEMORY;
    return r == MA_ACCESS_DENIED ? MOCKINGBIRD_ERR_PERMISSION : MOCKINGBIRD_ERR_BACKEND;
}

}

MiniaudioOutput::MiniaudioOutput() : impl_(std::make_unique<Impl>()) {}

MiniaudioOutput::~MiniaudioOutput() {
    impl_->stopping.store(true, std::memory_order_release);
    if (impl_->initialized) ma_device_uninit(&impl_->device);
}

mockingbird_status_t MiniaudioOutput::init(const OutputFormat& fmt, RenderCallback cb, void* user,
                                           OutputNoticeCallback notice, void* notice_user) {
    if (impl_->initialized) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (cb == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    impl_->cb          = cb;
    impl_->user        = user;
    impl_->notice      = notice;
    impl_->notice_user = notice_user;
    ma_device_config dc     = ma_device_config_init(ma_device_type_playback);
    dc.playback.format      = ma_format_s16;
    dc.playback.channels    = fmt.channels;
    dc.sampleRate           = fmt.sample_rate_hz;
    dc.periodSizeInFrames   = fmt.period_frames;
    dc.dataCallback         = data_callback;
    dc.notificationCallback = notification;
    dc.pUserData            = impl_.get();
    const ma_result r = ma_device_init(nullptr, &dc, &impl_->device);
    if (r != MA_SUCCESS) return map_result(r);
    impl_->initialized = true;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t MiniaudioOutput::start() {
    if (!impl_->initialized) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (impl_->running) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
    impl_->stopping.store(false, std::memory_order_release);
    const ma_result r = ma_device_start(&impl_->device);
    if (r != MA_SUCCESS) return map_result(r);
    impl_->running = true;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t MiniaudioOutput::stop() {
    if (!impl_->running) return MOCKINGBIRD_OK;
    impl_->stopping.store(true, std::memory_order_release);
    ma_device_stop(&impl_->device);
    impl_->running = false;
    return MOCKINGBIRD_OK;
}

}

#endif
