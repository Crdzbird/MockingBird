// Core Audio backend lifecycle.
#include "capture/coreaudio_backend.hpp"

#if defined(__APPLE__)

#include "capture/coreaudio_impl.hpp"

#if TARGET_OS_IPHONE
#import <AVFoundation/AVFoundation.h>
#endif

namespace auris {

CoreAudioBackend::CoreAudioBackend() : impl_(std::make_unique<Impl>()) {}

CoreAudioBackend::~CoreAudioBackend() {
    if (impl_->running) AudioOutputUnitStop(impl_->unit);
    if (impl_->initialized) {
        AudioUnitUninitialize(impl_->unit);
        AudioComponentInstanceDispose(impl_->unit);
    }
}

auris_status_t CoreAudioBackend::init(const auris_config_t& cfg, PcmCallback cb, void* user) {
    if (impl_->initialized) return AURIS_ERR_INVALID_STATE;
    if (cb == nullptr) return AURIS_ERR_INVALID_ARG;

    impl_->cb          = cb;
    impl_->user        = user;
    impl_->sample_rate = cfg.sample_rate_hz;
    impl_->channels    = cfg.channels;

    const auris_status_t opened = coreaudio_open_unit(*impl_, cfg);
    if (opened != AURIS_OK) return opened;

    AURenderCallbackStruct cbStruct{};
    cbStruct.inputProc       = coreaudio_input_render;
    cbStruct.inputProcRefCon = impl_.get();
    OSStatus s = AudioUnitSetProperty(impl_->unit, kAudioOutputUnitProperty_SetInputCallback,
                                      kAudioUnitScope_Global, kCoreAudioInputBus,
                                      &cbStruct, sizeof(cbStruct));
    if (s == noErr) s = AudioUnitInitialize(impl_->unit);
    if (s != noErr) {
        AudioComponentInstanceDispose(impl_->unit);
        impl_->unit = nullptr;
        return AURIS_ERR_BACKEND;
    }
    impl_->initialized = true;
    return AURIS_OK;
}

auris_status_t CoreAudioBackend::start() {
    if (!impl_->initialized) return AURIS_ERR_INVALID_STATE;
    if (impl_->running) return AURIS_ERR_ALREADY_RUNNING;
    if (AudioOutputUnitStart(impl_->unit) != noErr) return AURIS_ERR_BACKEND;
    impl_->running = true;
    return AURIS_OK;
}

auris_status_t CoreAudioBackend::stop() {
    if (!impl_->running) return AURIS_OK;
    AudioOutputUnitStop(impl_->unit);
    impl_->running = false;
#if TARGET_OS_IPHONE
    [[AVAudioSession sharedInstance]
        setActive:NO
      withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
            error:nil];
#endif
    return AURIS_OK;
}

} // namespace auris

#endif
