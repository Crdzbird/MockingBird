// miniaudio capture backend. The library implementation is compiled once in
// miniaudio_impl.c; this unit only uses the API.
#include "capture/miniaudio_backend.hpp"

#include "capture/chunk_timestamp.hpp"
#include "miniaudio.h"

#include <cstdint>

namespace auris {

namespace {

auris_status_t map_ma_result(ma_result r) noexcept {
    switch (r) {
        case MA_SUCCESS:                return AURIS_OK;
        case MA_ACCESS_DENIED:          return AURIS_ERR_PERMISSION;
        case MA_NO_DEVICE:              return AURIS_ERR_BACKEND;
        case MA_DEVICE_NOT_INITIALIZED: return AURIS_ERR_INVALID_STATE;
        case MA_INVALID_ARGS:           return AURIS_ERR_INVALID_ARG;
        case MA_OUT_OF_MEMORY:          return AURIS_ERR_NO_MEMORY;
        default:                        return AURIS_ERR_BACKEND;
    }
}

} // namespace

struct MiniaudioBackend::Impl {
    ma_device   device{};
    bool        device_initialized = false;
    bool        running            = false;
    PcmCallback cb                 = nullptr;
    void*       user               = nullptr;
    uint32_t    sample_rate        = 0;
};

static void miniaudio_data_callback(ma_device* device, void*, const void* input,
                                    ma_uint32 frame_count) {
    auto* impl = static_cast<MiniaudioBackend::Impl*>(device->pUserData);
    if (impl == nullptr || impl->cb == nullptr || input == nullptr || frame_count == 0) return;
    impl->cb(impl->user, static_cast<const int16_t*>(input), frame_count,
             chunk_start_ts_us(impl->sample_rate, frame_count));
}

MiniaudioBackend::MiniaudioBackend() : impl_(std::make_unique<Impl>()) {}

MiniaudioBackend::~MiniaudioBackend() {
    if (impl_->running) ma_device_stop(&impl_->device);
    if (impl_->device_initialized) ma_device_uninit(&impl_->device);
}

auris_status_t MiniaudioBackend::init(const auris_config_t& cfg, PcmCallback cb, void* user) {
    if (impl_->device_initialized) return AURIS_ERR_INVALID_STATE;
    if (cb == nullptr) return AURIS_ERR_INVALID_ARG;

    impl_->cb          = cb;
    impl_->user        = user;
    impl_->sample_rate = cfg.sample_rate_hz;

    ma_device_config dc   = ma_device_config_init(ma_device_type_capture);
    dc.capture.format     = ma_format_s16;
    dc.capture.channels   = cfg.channels;
    dc.sampleRate         = cfg.sample_rate_hz;
    dc.periodSizeInFrames = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
    dc.dataCallback       = miniaudio_data_callback;
    dc.pUserData          = impl_.get();

    const ma_result r = ma_device_init(nullptr, &dc, &impl_->device);
    if (r != MA_SUCCESS) return map_ma_result(r);
    impl_->device_initialized = true;
    return AURIS_OK;
}

auris_status_t MiniaudioBackend::start() {
    if (!impl_->device_initialized) return AURIS_ERR_INVALID_STATE;
    if (impl_->running) return AURIS_ERR_ALREADY_RUNNING;
    const ma_result r = ma_device_start(&impl_->device);
    if (r != MA_SUCCESS) return map_ma_result(r);
    impl_->running = true;
    return AURIS_OK;
}

auris_status_t MiniaudioBackend::stop() {
    if (!impl_->running) return AURIS_OK;
    ma_device_stop(&impl_->device);
    impl_->running = false;
    return AURIS_OK;
}

} // namespace auris
