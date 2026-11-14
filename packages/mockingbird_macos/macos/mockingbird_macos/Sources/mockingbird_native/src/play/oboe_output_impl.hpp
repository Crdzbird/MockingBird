// The Oboe output's stream and callbacks: a low-latency int16 output stream
// with Oboe's resampler, so the player's rate plays on any device rate. A
// stream that disconnects (headphones, Bluetooth, USB) is reopened on the new
// route from Oboe's error callback, where Oboe allows it. Each stream holds
// the callbacks by shared_ptr, so Oboe's error thread, which keeps its stream
// alive, can never reach a freed Impl; `running` is cleared under the mutex
// before teardown, so that thread never calls into a player being destroyed.
#pragma once

#include "play/oboe_output.hpp"

#ifdef MOCKINGBIRD_HAVE_OBOE

#include <oboe/Oboe.h>

#include <memory>
#include <mutex>

namespace mockingbird {
struct OboeOutput::Impl : public oboe::AudioStreamDataCallback,
                          public oboe::AudioStreamErrorCallback,
                          public std::enable_shared_from_this<OboeOutput::Impl> {
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
        const std::shared_ptr<Impl> self = shared_from_this();
        b.setDataCallback(self);
        b.setErrorCallback(self);
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

}

#endif
