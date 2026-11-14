// miniaudio playback backend lifecycle on the default output device.
#ifndef __EMSCRIPTEN__

#include "play/miniaudio_output.hpp"

#include "play/miniaudio_output_impl.hpp"

namespace mockingbird {
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
    dc.dataCallback         = miniaudio_output_data;
    dc.notificationCallback = miniaudio_output_notice;
    dc.pUserData            = impl_.get();
    const ma_result r = ma_device_init(nullptr, &dc, &impl_->device);
    if (r != MA_SUCCESS) return miniaudio_output_status(r);
    impl_->initialized = true;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t MiniaudioOutput::start() {
    if (!impl_->initialized) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (impl_->running.load(std::memory_order_acquire)) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
    impl_->stopping.store(false, std::memory_order_release);
    const ma_result r = ma_device_start(&impl_->device);
    if (r != MA_SUCCESS) return miniaudio_output_status(r);
    impl_->running.store(true, std::memory_order_release);
    return MOCKINGBIRD_OK;
}

mockingbird_status_t MiniaudioOutput::stop() {
    impl_->stopping.store(true, std::memory_order_release);
    if (!impl_->running.exchange(false, std::memory_order_acq_rel)) return MOCKINGBIRD_OK;
    ma_device_stop(&impl_->device);
    return MOCKINGBIRD_OK;
}

}

#endif
