// src/capture/miniaudio_backend.cpp

#include "miniaudio_backend.hpp"
#include "coreaudio_backend.hpp"
#include "oboe_backend.hpp"

// Pull the miniaudio API (NOT the implementation; that lives in
// miniaudio_impl.c so the 70k-LoC header is compiled exactly once).
#include "miniaudio.h"

#include <chrono>
#include <cstdint>

namespace auris {

namespace {

uint64_t monotonic_now_us() noexcept {
    using clock = std::chrono::steady_clock;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            clock::now().time_since_epoch()).count());
}

// Map a few well-known ma_result codes; the rest collapse to AURIS_ERR_BACKEND.
// // VERIFY  miniaudio's ma_result taxonomy. Expand as we discover more codes
// in real-world failures.
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

// Free function (C linkage not required; miniaudio takes a void* to anything
// callable as ma_device_data_proc).
static void miniaudio_data_callback(ma_device*  device,
                                    void* /*output*/,
                                    const void* input,
                                    ma_uint32   frame_count) {
    auto* impl = static_cast<MiniaudioBackend::Impl*>(device->pUserData);
    if (impl == nullptr || impl->cb == nullptr || input == nullptr || frame_count == 0) {
        return;
    }

    // The chunk's first-sample timestamp is now() minus chunk duration.
    // // ASSUMES  steady_clock is the same monotonic source the worker uses
    // for stamping segment events. Good enough until we plumb the OS's own
    // capture host time (Core Audio AudioTimeStamp / AAudio framePosition)
    // through miniaudio in a later step.
    const uint64_t now      = monotonic_now_us();
    const uint64_t chunk_us = (impl->sample_rate > 0)
        ? (static_cast<uint64_t>(frame_count) * 1'000'000ULL / impl->sample_rate)
        : 0;
    const uint64_t ts_first = (now > chunk_us) ? (now - chunk_us) : 0;

    impl->cb(impl->user,
             static_cast<const int16_t*>(input),
             frame_count,
             ts_first);
}

MiniaudioBackend::MiniaudioBackend() : impl_(std::make_unique<Impl>()) {}

MiniaudioBackend::~MiniaudioBackend() {
    // Defensive teardown if the caller forgot to stop().
    if (impl_->running) {
        ma_device_stop(&impl_->device);
        impl_->running = false;
    }
    if (impl_->device_initialized) {
        ma_device_uninit(&impl_->device);
        impl_->device_initialized = false;
    }
}

auris_status_t MiniaudioBackend::init(const auris_config_t& cfg,
                                      PcmCallback           cb,
                                      void*                 user) {
    if (impl_->device_initialized) return AURIS_ERR_INVALID_STATE;
    if (cb == nullptr)             return AURIS_ERR_INVALID_ARG;

    impl_->cb          = cb;
    impl_->user        = user;
    impl_->sample_rate = cfg.sample_rate_hz;

    ma_device_config dc       = ma_device_config_init(ma_device_type_capture);
    dc.capture.format         = ma_format_s16;
    dc.capture.channels       = cfg.channels;
    dc.sampleRate             = cfg.sample_rate_hz;
    // Align the data-callback period with our detector frame size. miniaudio
    // may round; the worker tolerates variable-sized pushes.
    dc.periodSizeInFrames     = (cfg.sample_rate_hz / 1000u) * cfg.frame_ms;
    dc.dataCallback           = miniaudio_data_callback;
    dc.pUserData              = impl_.get();

    const ma_result r = ma_device_init(nullptr, &dc, &impl_->device);
    if (r != MA_SUCCESS) {
        return map_ma_result(r);
    }
    impl_->device_initialized = true;
    return AURIS_OK;
}

auris_status_t MiniaudioBackend::start() {
    if (!impl_->device_initialized) return AURIS_ERR_INVALID_STATE;
    if (impl_->running)             return AURIS_ERR_ALREADY_RUNNING;

    const ma_result r = ma_device_start(&impl_->device);
    if (r != MA_SUCCESS) {
        return map_ma_result(r);
    }
    impl_->running = true;
    return AURIS_OK;
}

auris_status_t MiniaudioBackend::stop() {
    if (!impl_->running) return AURIS_OK;
    // ma_device_stop blocks until the data callback has finished firing, so
    // it is safe for the engine to then tear down the worker without racing
    // a half-completed push.
    ma_device_stop(&impl_->device);
    impl_->running = false;
    return AURIS_OK;
}

namespace {
BackendFactoryFn g_backend_override = nullptr;
}

void set_backend_factory_override(BackendFactoryFn fn) {
    g_backend_override = fn;
}

std::unique_ptr<CaptureBackend> make_backend(auris_backend_t which) {
    if (g_backend_override != nullptr) {
        return g_backend_override(which);   // // TEST-SEAM
    }
    switch (which) {
        case AURIS_BACKEND_AUTO:
        case AURIS_BACKEND_MINIAUDIO:
            return std::make_unique<MiniaudioBackend>();
        case AURIS_BACKEND_COREAUDIO:
#if defined(__APPLE__)
            return std::make_unique<CoreAudioBackend>();
#else
            return nullptr;
#endif
        case AURIS_BACKEND_OBOE:
#ifdef AURIS_HAVE_OBOE
            return std::make_unique<OboeBackend>();
#else
            // Fall through to miniaudio (which uses AAudio / OpenSL ES on
            // Android internally) so the demo still works on platforms
            // where Oboe is not available.
            return std::make_unique<MiniaudioBackend>();
#endif
        case AURIS_BACKEND_HOST_PCM:
        default:
            return nullptr;
    }
}

} // namespace auris
