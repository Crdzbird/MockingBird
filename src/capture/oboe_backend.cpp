// src/capture/oboe_backend.cpp

#include "oboe_backend.hpp"

#ifdef AURIS_HAVE_OBOE

#include <oboe/Oboe.h>

#include <chrono>
#include <cstdint>
#include <memory>

namespace auris {

namespace {

uint64_t monotonic_now_us() noexcept {
    using clock = std::chrono::steady_clock;
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            clock::now().time_since_epoch()).count());
}

} // namespace

struct OboeBackend::Impl : public oboe::AudioStreamDataCallback,
                           public oboe::AudioStreamErrorCallback {
    std::shared_ptr<oboe::AudioStream> stream;
    PcmCallback                        cb         = nullptr;
    void*                              user       = nullptr;
    uint32_t                           sample_rate = 0;
    bool                               running    = false;

    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* /*as*/,
                                          void*              audioData,
                                          int32_t            numFrames) override {
        if (cb != nullptr && audioData != nullptr && numFrames > 0) {
            // // PERF-CRITICAL  RT capture thread. No allocation or logging.
            // Oboe exposes a presentation timestamp via getTimestamp(); the
            // call is cheap but locks an internal mutex, so we settle for
            // steady_clock-derived chunk-start time for now. A future
            // refinement can swap in the OS-host timestamp.
            const uint32_t chunk_us = (sample_rate > 0)
                ? static_cast<uint32_t>(static_cast<uint64_t>(numFrames) * 1'000'000ULL / sample_rate)
                : 0u;
            const uint64_t now      = monotonic_now_us();
            const uint64_t ts_first = (now > chunk_us) ? (now - chunk_us) : 0;
            cb(user,
               static_cast<const int16_t*>(audioData),
               static_cast<uint32_t>(numFrames),
               ts_first);
        }
        return oboe::DataCallbackResult::Continue;
    }

    void onErrorAfterClose(oboe::AudioStream* /*as*/,
                           oboe::Result        /*err*/) override {
        // Stream auto-recovers below; nothing to do here for now.
    }
};

OboeBackend::OboeBackend() : impl_(std::make_unique<Impl>()) {}

OboeBackend::~OboeBackend() {
    if (impl_->running && impl_->stream) {
        impl_->stream->stop();
        impl_->running = false;
    }
    if (impl_->stream) {
        impl_->stream->close();
        impl_->stream.reset();
    }
}

auris_status_t OboeBackend::init(const auris_config_t& cfg,
                                 PcmCallback           cb,
                                 void*                 user) {
    if (impl_->stream != nullptr) return AURIS_ERR_INVALID_STATE;
    if (cb == nullptr)            return AURIS_ERR_INVALID_ARG;

    impl_->cb          = cb;
    impl_->user        = user;
    impl_->sample_rate = cfg.sample_rate_hz;

    // // VERIFY  Oboe's AudioStreamBuilder setters return pointers, not
    // references, so chaining with `.` does not compile. Use sequential
    // calls instead; each setter modifies the builder in place.
    oboe::AudioStreamBuilder b;
    b.setDirection(oboe::Direction::Input);
    b.setPerformanceMode(oboe::PerformanceMode::LowLatency);
    b.setSharingMode(oboe::SharingMode::Shared);
    b.setFormat(oboe::AudioFormat::I16);
    b.setChannelCount(static_cast<int>(cfg.channels));
    b.setSampleRate(static_cast<int32_t>(cfg.sample_rate_hz));
    b.setFramesPerDataCallback(
        static_cast<int32_t>((cfg.sample_rate_hz / 1000u) * cfg.frame_ms));
    b.setDataCallback(impl_.get());
    b.setErrorCallback(impl_.get());
    b.setInputPreset(oboe::InputPreset::VoiceRecognition);

    std::shared_ptr<oboe::AudioStream> stream;
    const oboe::Result r = b.openStream(stream);
    if (r != oboe::Result::OK) {
        // // VERIFY  Refine ma_result-style mapping for Oboe's set of error
        // codes. For now, permission-denied + everything else collapse to
        // BACKEND.
        return AURIS_ERR_BACKEND;
    }
    impl_->stream = std::move(stream);
    return AURIS_OK;
}

auris_status_t OboeBackend::start() {
    if (!impl_->stream)        return AURIS_ERR_INVALID_STATE;
    if (impl_->running)        return AURIS_ERR_ALREADY_RUNNING;
    const oboe::Result r = impl_->stream->requestStart();
    if (r != oboe::Result::OK) return AURIS_ERR_BACKEND;
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

#endif // AURIS_HAVE_OBOE
