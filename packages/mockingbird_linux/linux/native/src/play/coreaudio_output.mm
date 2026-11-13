// Core Audio output lifecycle. The render callback runs on the OS audio
// thread and hands the unit's interleaved int16 buffer straight to the
// player; start and stop take the shared iOS audio session for playback.
#include "play/coreaudio_output.hpp"

#if defined(__APPLE__)

#include "platform/apple_audio_session.hpp"
#include "play/coreaudio_output_impl.hpp"

namespace mockingbird {
namespace {
OSStatus render(void* ref, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32 frames,
                AudioBufferList* io) {
    auto* impl = static_cast<CoreAudioOutput::Impl*>(ref);
    if (io == nullptr || io->mNumberBuffers < 1 || io->mBuffers[0].mData == nullptr) return noErr;
    const UInt32 need = frames * impl->channels * sizeof(int16_t);
    if (io->mBuffers[0].mDataByteSize < need) return kAudioUnitErr_TooManyFramesToProcess;
    impl->cb(impl->user, static_cast<int16_t*>(io->mBuffers[0].mData), frames);
    return noErr;
}

AudioComponentInstance create_unit() {
    AudioComponentDescription desc{};
    desc.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
#else
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    AudioComponentInstance unit = nullptr;
    if (comp == nullptr || AudioComponentInstanceNew(comp, &unit) != noErr) return nullptr;
    return unit;
}

OSStatus configure(AudioComponentInstance unit, const OutputFormat& fmt, CoreAudioOutput::Impl* impl) {
    AudioStreamBasicDescription asbd{};
    asbd.mSampleRate       = fmt.sample_rate_hz;
    asbd.mFormatID         = kAudioFormatLinearPCM;
    asbd.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked |
                             kAudioFormatFlagsNativeEndian;
    asbd.mFramesPerPacket  = 1;
    asbd.mChannelsPerFrame = fmt.channels;
    asbd.mBitsPerChannel   = 16;
    asbd.mBytesPerFrame    = static_cast<UInt32>(fmt.channels) * 2u;
    asbd.mBytesPerPacket   = asbd.mBytesPerFrame;
    OSStatus s = AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                                      &asbd, sizeof(asbd));
    AURenderCallbackStruct cb{render, impl};
    if (s == noErr) {
        s = AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                                 &cb, sizeof(cb));
    }
    return s == noErr ? AudioUnitInitialize(unit) : s;
}

}

bool CoreAudioOutput::Impl::start_unit() {
    if (running) return true;
    running = AudioOutputUnitStart(unit) == noErr;
    return running;
}

void CoreAudioOutput::Impl::stop_unit() {
    if (running) AudioOutputUnitStop(unit);
    running = false;
}

CoreAudioOutput::CoreAudioOutput() : impl_(std::make_shared<Impl>()) {}

CoreAudioOutput::~CoreAudioOutput() {
    coreaudio_output_unwatch(*impl_);
    std::lock_guard lk(impl_->mu);
    impl_->alive = false;
    impl_->stop_unit();
    if (impl_->unit != nullptr) {
        AudioUnitUninitialize(impl_->unit);
        AudioComponentInstanceDispose(impl_->unit);
        impl_->unit = nullptr;
    }
    if (impl_->session_held) audio_session_release(SessionUse::Playback);
}

mockingbird_status_t CoreAudioOutput::init(const OutputFormat& fmt, RenderCallback cb, void* user,
                                           OutputNoticeCallback notice, void* notice_user) {
    if (impl_->unit != nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (cb == nullptr) return MOCKINGBIRD_ERR_INVALID_ARG;
    impl_->cb          = cb;
    impl_->user        = user;
    impl_->notice      = notice;
    impl_->notice_user = notice_user;
    impl_->channels    = fmt.channels;
    AudioComponentInstance unit = create_unit();
    if (unit == nullptr) return MOCKINGBIRD_ERR_BACKEND;
    if (configure(unit, fmt, impl_.get()) != noErr) {
        AudioComponentInstanceDispose(unit);
        return MOCKINGBIRD_ERR_BACKEND;
    }
    impl_->unit = unit;
    coreaudio_output_watch(impl_);
    return MOCKINGBIRD_OK;
}

mockingbird_status_t CoreAudioOutput::start() {
    std::lock_guard lk(impl_->mu);
    if (impl_->unit == nullptr) return MOCKINGBIRD_ERR_INVALID_STATE;
    if (impl_->running) return MOCKINGBIRD_ERR_ALREADY_RUNNING;
    if (!impl_->session_held) {
        const mockingbird_status_t s = audio_session_acquire(SessionUse::Playback, 0);
        if (s != MOCKINGBIRD_OK) return s;
        impl_->session_held = true;
    }
    if (!impl_->start_unit()) return MOCKINGBIRD_ERR_BACKEND;
    impl_->wanted = true;
    return MOCKINGBIRD_OK;
}

mockingbird_status_t CoreAudioOutput::stop() {
    std::lock_guard lk(impl_->mu);
    impl_->wanted = false;
    impl_->stop_unit();
    if (impl_->session_held) audio_session_release(SessionUse::Playback);
    impl_->session_held = false;
    return MOCKINGBIRD_OK;
}

}

#endif
