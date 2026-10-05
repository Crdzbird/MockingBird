// Oboe capture backend for Android.
#include "capture/oboe_backend.hpp"

#ifdef AURIS_HAVE_OBOE

#include "capture/chunk_timestamp.hpp"

#include <oboe/Oboe.h>

#include <cstdint>
#include <memory>

namespace auris {

struct OboeBackend::Impl : public oboe::AudioStreamDataCallback,
                           public oboe::AudioStreamErrorCallback {
    std::shared_ptr<oboe::AudioStream> stream;
    PcmCallback                        cb          = nullptr;
    void*                              user        = nullptr;
    uint32_t                           sample_rate = 0;
    bool                               running     = false;

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* audioData,
                                          int32_t numFrames) override {
        if (cb != nullptr && audioData != nullptr && numFrames > 0) {
            const auto frames = static_cast<uint32_t>(numFrames);
            cb(user, static_cast<const int16_t*>(audioData), frames,
               chunk_start_ts_us(sample_rate, frames));
        }
        return oboe::DataCallbackResult::Continue;
    }

    void onErrorAfterClose(oboe::AudioStream*, oboe::Result) override {}
};

OboeBackend::OboeBackend() : impl_(std::make_unique<Impl>()) {}

OboeBackend::~OboeBackend() {
    if (impl_->running && impl_->stream) impl_->stream->stop();
    if (impl_->stream) impl_->stream->close();
}

auris_status_t OboeBackend::init(const auris_config_t& cfg, PcmCallback cb, void* user) {
    if (impl_->stream != nullptr) return AURIS_ERR_INVALID_STATE;
    if (cb == nullptr) return AURIS_ERR_INVALID_ARG;

    impl_->cb          = cb;
    impl_->user        = user;
    impl_->sample_rate = cfg.sample_rate_hz;

    oboe::AudioStreamBuilder b;
    b.setDirection(oboe::Direction::Input);
    b.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    b.setSharingMode(oboe::SharingMode::Shared);
    b.setFormat(oboe::AudioFormat::I16);
    b.setChannelCount(static_cast<int>(cfg.channels));
    b.setSampleRate(static_cast<int32_t>(cfg.sample_rate_hz));
    b.setFramesPerDataCallback(static_cast<int32_t>((cfg.sample_rate_hz / 1000u) * cfg.frame_ms));
    b.setDataCallback(impl_.get());
    b.setErrorCallback(impl_.get());
    b.setInputPreset(oboe::InputPreset::VoiceRecognition);

    std::shared_ptr<oboe::AudioStream> stream;
    if (b.openStream(stream) != oboe::Result::OK) return AURIS_ERR_BACKEND;
    impl_->stream = std::move(stream);
    return AURIS_OK;
}

auris_status_t OboeBackend::start() {
    if (!impl_->stream) return AURIS_ERR_INVALID_STATE;
    if (impl_->running) return AURIS_ERR_ALREADY_RUNNING;
    if (impl_->stream->requestStart() != oboe::Result::OK) return AURIS_ERR_BACKEND;
    impl_->running = true;
    return AURIS_OK;
}

auris_status_t OboeBackend::stop() {
    if (!impl_->running) return AURIS_OK;
    impl_->stream->requestStop();
    impl_->running = false;
    return AURIS_OK;
}

} // namespace auris

#endif
