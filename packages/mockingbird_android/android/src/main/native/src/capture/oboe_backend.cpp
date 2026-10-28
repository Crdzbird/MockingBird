// Oboe capture backend for Android. A disconnected stream (a headset plugged
// or unplugged, a USB microphone removed) is reopened on the new route from
// Oboe's error callback, which is where Oboe allows it. Voice processing
// switches to the VOICE_COMMUNICATION input preset, which applies the
// device's echo canceller, gain control and noise suppressor where it has
// them; a selected device is an AudioDeviceInfo id.
#include "capture/oboe_backend.hpp"

#ifdef MOCKINGBIRD_HAVE_OBOE

#include "capture/chunk_timestamp.hpp"

#include <oboe/Oboe.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>

namespace mockingbird {
struct OboeBackend::Impl : public oboe::AudioStreamDataCallback,
                           public oboe::AudioStreamErrorCallback {
    std::shared_ptr<oboe::AudioStream> stream;
    PcmCallback                        cb          = nullptr;
    void*                              user        = nullptr;
    CaptureNoticeCallback              notice      = nullptr;
    void*                              notice_user = nullptr;
    mockingbird_config_t               cfg{};
    int32_t                            device_id   = oboe::kUnspecified;
    bool                               running     = false;
    std::mutex                         mu;

    void tell(mockingbird_event_type_t type) {
        if (notice != nullptr) notice(notice_user, type, 0);
    }

    oboe::Result open() {
        oboe::AudioStreamBuilder b;
        b.setDirection(oboe::Direction::Input);
        b.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        b.setSharingMode(oboe::SharingMode::Shared);
        b.setFormat(oboe::AudioFormat::I16);
        b.setChannelCount(static_cast<int>(cfg.channels));
        b.setSampleRate(static_cast<int32_t>(cfg.sample_rate_hz));
        b.setFramesPerDataCallback(static_cast<int32_t>((cfg.sample_rate_hz / 1000u) * cfg.frame_ms));
        b.setDataCallback(this);
        b.setErrorCallback(this);
        b.setDeviceId(device_id);
        b.setInputPreset(cfg.voice_processing != 0 ? oboe::InputPreset::VoiceCommunication
                                                   : oboe::InputPreset::VoiceRecognition);
        return b.openStream(stream);
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* audioData,
                                          int32_t numFrames) override {
        if (cb != nullptr && audioData != nullptr && numFrames > 0) {
            const auto frames = static_cast<uint32_t>(numFrames);
            cb(user, static_cast<const int16_t*>(audioData), frames,
               chunk_start_ts_us(cfg.sample_rate_hz, frames));
        }
        return oboe::DataCallbackResult::Continue;
    }

    void onErrorAfterClose(oboe::AudioStream*, oboe::Result error) override {
        std::lock_guard lk(mu);
        if (!running) return;
        if (error == oboe::Result::ErrorDisconnected && open() == oboe::Result::OK &&
            stream->requestStart() == oboe::Result::OK) {
            tell(MOCKINGBIRD_EVT_ROUTE_CHANGED);
            return;
        }
        running = false;
        tell(MOCKINGBIRD_EVT_CAPTURE_INTERRUPTED);
    }
};

OboeBackend::OboeBackend() : impl_(std::make_unique<Impl>()) {}

OboeBackend::~OboeBackend() {
    std::shared_ptr<oboe::AudioStream> s;
    {
        std::lock_guard lk(impl_->mu);
        impl_->running = false;
        s = impl_->stream;
    }
    if (s) {
        s->stop();
        s->close();
    }
}

void OboeBackend::set_notice_callback(CaptureNoticeCallback cb, void* user) {
    impl_->notice      = cb;
    impl_->notice_user = user;
}

mockingbird_status_t OboeBackend::select_device(const std::string& id) {
    if (impl_->stream != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (id.empty()) {
        impl_->device_id = oboe::kUnspecified;
        return MOCKINGBIRD_OK;
    }
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(id.c_str(), &end, 10);
    if (errno != 0 || end == id.c_str() || *end != '\0' || v <= 0 || v > INT32_MAX) {
        return MOCKINGBIRD_ERR_INVALID_ARG;
    }
    impl_->device_id = static_cast<int32_t>(v);
    return MOCKINGBIRD_OK;
}

uint32_t OboeBackend::voice_processing_active() const {
    return impl_->cfg.voice_processing &
           (MOCKINGBIRD_VOICE_ECHO_CANCEL | MOCKINGBIRD_VOICE_GAIN_CONTROL |
            MOCKINGBIRD_VOICE_NOISE_SUPPRESS);
}

mockingbird_status_t OboeBackend::init(const mockingbird_config_t& cfg, PcmCallback cb, void* user) {
    if (impl_->stream != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (cb == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    impl_->cb   = cb;
    impl_->user = user;
    impl_->cfg  = cfg;
    return impl_->open() == oboe::Result::OK ? MOCKINGBIRD_OK : MOCKINGBIRD_ERR_BACKEND;
}

mockingbird_status_t OboeBackend::start() {
    std::lock_guard lk(impl_->mu);
    if (!impl_->stream) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (impl_->running) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
    if (impl_->stream->requestStart() != oboe::Result::OK) return MOCKINGBIRD_ERR_BACKEND;
    impl_->running = true;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t OboeBackend::stop() {
    std::shared_ptr<oboe::AudioStream> s;
    {
        std::lock_guard lk(impl_->mu);
        if (!impl_->running) return MOCKINGBIRD_OK;
        impl_->running = false;
        s = impl_->stream;
    }
    if (s) s->requestStop();
    return MOCKINGBIRD_OK;
}

}

#endif
