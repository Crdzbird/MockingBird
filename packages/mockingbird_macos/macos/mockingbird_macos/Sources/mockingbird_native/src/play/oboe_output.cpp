// Oboe playback backend for Android: a low-latency int16 output stream with
// Oboe's resampler, so the player's rate plays on any device rate. A stream
// that disconnects (headphones, Bluetooth, USB) is reopened on the new route
// from Oboe's error callback, where Oboe allows it. Audio focus and the
// becoming-noisy broadcast are Java APIs and live in the Flutter plugin.
#include "play/oboe_output.hpp"

#ifdef MOCKINGBIRD_HAVE_OBOE

#include <oboe/Oboe.h>

#include <mutex>

namespace mockingbird {
struct OboeOutput::Impl : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
    std::shared_ptr<oboe::AudioStream> stream;
    OutputFormat                       fmt{};
    RenderCallback                     cb          = nullptr;
    void*                              user        = nullptr;
    OutputNoticeCallback               notice      = nullptr;
    void*                              notice_user = nullptr;
    bool                               running     = false;
    std::mutex                         mu;

    void tell(OutputNotice n) const {
        if (notice != nullptr) notice(notice_user, n);
    }

    oboe::Result open() {
        oboe::AudioStreamBuilder b;
        b.setDirection(oboe::Direction::Output);
        b.setPerformanceMode(oboe::PerformanceMode::LowLatency);
        b.setSharingMode(oboe::SharingMode::Shared);
        b.setFormat(oboe::AudioFormat::I16);
        b.setChannelCount(static_cast<int>(fmt.channels));
        b.setSampleRate(static_cast<int32_t>(fmt.sample_rate_hz));
        b.setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium);
        b.setUsage(oboe::Usage::Media);
        b.setContentType(oboe::ContentType::Speech);
        b.setDataCallback(this);
        b.setErrorCallback(this);
        return b.openStream(stream);
    }

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream*, void* audioData, int32_t numFrames) override {
        if (audioData != nullptr && numFrames > 0) {
            cb(user, static_cast<int16_t*>(audioData), static_cast<uint32_t>(numFrames));
        }
        return oboe::DataCallbackResult::Continue;
    }

    void onErrorAfterClose(oboe::AudioStream*, oboe::Result error) override {
        std::lock_guard lk(mu);
        if (!running) return;
        if (error == oboe::Result::ErrorDisconnected && open() == oboe::Result::OK &&
            stream->requestStart() == oboe::Result::OK) {
            tell(OutputNotice::RouteChanged);
            return;
        }
        running = false;
        tell(OutputNotice::Interrupted);
    }
};

OboeOutput::OboeOutput() : impl_(std::make_unique<Impl>()) {}

OboeOutput::~OboeOutput() {
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

mockingbird_status_t OboeOutput::init(const OutputFormat& fmt, RenderCallback cb, void* user,
                                      OutputNoticeCallback notice, void* notice_user) {
    if (impl_->stream != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (cb == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    impl_->fmt         = fmt;
    impl_->cb          = cb;
    impl_->user        = user;
    impl_->notice      = notice;
    impl_->notice_user = notice_user;
    return impl_->open() == oboe::Result::OK ? MOCKINGBIRD_OK : MOCKINGBIRD_ERR_BACKEND;
}

mockingbird_status_t OboeOutput::start() {
    std::lock_guard lk(impl_->mu);
    if (!impl_->stream) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (impl_->running) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
    if (impl_->stream->getState() == oboe::StreamState::Closed && impl_->open() != oboe::Result::OK) {
        return MOCKINGBIRD_ERR_BACKEND;
    }
    if (impl_->stream->requestStart() != oboe::Result::OK) return MOCKINGBIRD_ERR_BACKEND;
    impl_->running = true;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t OboeOutput::stop() {
    std::shared_ptr<oboe::AudioStream> s;
    {
        std::lock_guard lk(impl_->mu);
        if (!impl_->running) return MOCKINGBIRD_OK;
        impl_->running = false;
        s = impl_->stream;
    }
    if (s) s->stop();
    return MOCKINGBIRD_OK;
}

}

#endif
