// src/capture/coreaudio_backend.mm
//
// macOS / iOS native capture via AudioUnit. .mm so we can use #include
// <AudioUnit/AudioUnit.h> on iOS where the Objective-C bridging layer is
// required for AVAudioSession activation in a future step.

#include "coreaudio_backend.hpp"

#if defined(__APPLE__)

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
// macOS-only: default-input-device discovery for the HAL output unit.
#include <CoreAudio/AudioHardware.h>
#else
// iOS-only: RemoteIO capture requires an active AVAudioSession.
#import <AVFoundation/AVFoundation.h>
#endif
#include <mach/mach_time.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

namespace auris {

namespace {

// Cache mach timebase once for converting AudioTimeStamp.mHostTime to
// microseconds.
struct MachTimebase {
    mach_timebase_info_data_t info{};
    bool                       ready = false;
    MachTimebase() {
        if (mach_timebase_info(&info) == KERN_SUCCESS) ready = true;
    }
    uint64_t to_us(uint64_t host_time) const {
        if (!ready || info.denom == 0) return 0;
        // mHostTime is in mach absolute time. (host * numer / denom) is ns;
        // / 1000 gives us microseconds.
        const __uint128_t ns =
            static_cast<__uint128_t>(host_time) * info.numer / info.denom;
        return static_cast<uint64_t>(ns / 1000ull);
    }
};

const MachTimebase& timebase() {
    static const MachTimebase tb;
    return tb;
}

auris_status_t map_osstatus(OSStatus s) {
    if (s == noErr) return AURIS_OK;
    return AURIS_ERR_BACKEND;
}

} // namespace

struct CoreAudioBackend::Impl {
    AudioComponentInstance  unit          = nullptr;
    bool                    initialized   = false;
    bool                    running       = false;
    PcmCallback             cb            = nullptr;
    void*                   user          = nullptr;
    uint32_t                sample_rate   = 0;
    uint16_t                channels      = 1;
    // Scratch buffer the input render callback uses to receive samples
    // from AudioUnitRender before fanning them out to the engine.
    std::vector<int16_t>    scratch;
};

namespace {

OSStatus input_render_callback(void*                       refCon,
                               AudioUnitRenderActionFlags* ioActionFlags,
                               const AudioTimeStamp*       inTimeStamp,
                               UInt32                      inBusNumber,
                               UInt32                      inNumberFrames,
                               AudioBufferList*            /*ioData*/) {
    auto* impl = static_cast<CoreAudioBackend::Impl*>(refCon);
    if (impl == nullptr || impl->cb == nullptr) return noErr;

    const size_t needed = static_cast<size_t>(inNumberFrames) * impl->channels;
    if (impl->scratch.size() < needed) impl->scratch.resize(needed);

    AudioBufferList list{};
    list.mNumberBuffers              = 1;
    list.mBuffers[0].mNumberChannels = impl->channels;
    list.mBuffers[0].mDataByteSize   =
        static_cast<UInt32>(needed * sizeof(int16_t));
    list.mBuffers[0].mData           = impl->scratch.data();

    const OSStatus r = AudioUnitRender(impl->unit,
                                       ioActionFlags,
                                       inTimeStamp,
                                       inBusNumber,
                                       inNumberFrames,
                                       &list);
    if (r != noErr) return r;

    // // PERF-CRITICAL  Avoid logging or allocating; this runs on Apple's
    // RT audio thread.
    const uint64_t ts_us =
        (inTimeStamp != nullptr && (inTimeStamp->mFlags & kAudioTimeStampHostTimeValid))
            ? timebase().to_us(inTimeStamp->mHostTime)
            : 0;

    impl->cb(impl->user,
             impl->scratch.data(),
             inNumberFrames,
             ts_us);
    return noErr;
}

} // namespace

CoreAudioBackend::CoreAudioBackend() : impl_(std::make_unique<Impl>()) {}

CoreAudioBackend::~CoreAudioBackend() {
    if (impl_->running) {
        AudioOutputUnitStop(impl_->unit);
        impl_->running = false;
    }
    if (impl_->initialized) {
        AudioUnitUninitialize(impl_->unit);
        AudioComponentInstanceDispose(impl_->unit);
        impl_->initialized = false;
    }
}

auris_status_t CoreAudioBackend::init(const auris_config_t& cfg,
                                      PcmCallback           cb,
                                      void*                 user) {
    if (impl_->initialized) return AURIS_ERR_INVALID_STATE;
    if (cb == nullptr)       return AURIS_ERR_INVALID_ARG;

    impl_->cb          = cb;
    impl_->user        = user;
    impl_->sample_rate = cfg.sample_rate_hz;
    impl_->channels    = cfg.channels;

#if TARGET_OS_IPHONE
    // CRITICAL (iOS): a RemoteIO unit cannot capture until an AVAudioSession is
    // configured for recording and made active. Without this, AudioUnitInitialize
    // / AudioOutputUnitStart fail and auris_start returns AURIS_ERR_BACKEND.
    // macOS has no audio session, so this is iOS-only.
    {
        AVAudioSession* session = [AVAudioSession sharedInstance];
        NSError* sessErr = nil;
        if (![session setCategory:AVAudioSessionCategoryRecord
                            error:&sessErr]) {
            return AURIS_ERR_BACKEND;
        }
        [session setPreferredSampleRate:static_cast<double>(cfg.sample_rate_hz)
                                  error:nil];
        if (![session setActive:YES error:&sessErr]) {
            return AURIS_ERR_BACKEND;
        }
    }
#endif

    AudioComponentDescription desc{};
    desc.componentType         = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
    desc.componentSubType      = kAudioUnitSubType_RemoteIO;
#else
    desc.componentSubType      = kAudioUnitSubType_HALOutput;
#endif
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (comp == nullptr) return AURIS_ERR_BACKEND;

    OSStatus s = AudioComponentInstanceNew(comp, &impl_->unit);
    if (s != noErr) return map_osstatus(s);

    // Element 1 = input bus; element 0 = output bus.
    constexpr AudioUnitElement kInputBus  = 1;
    constexpr AudioUnitElement kOutputBus = 0;

    UInt32 one  = 1;
    UInt32 zero = 0;
    s = AudioUnitSetProperty(impl_->unit,
                             kAudioOutputUnitProperty_EnableIO,
                             kAudioUnitScope_Input,
                             kInputBus,
                             &one, sizeof(one));
    if (s != noErr) { AudioComponentInstanceDispose(impl_->unit); return map_osstatus(s); }

    s = AudioUnitSetProperty(impl_->unit,
                             kAudioOutputUnitProperty_EnableIO,
                             kAudioUnitScope_Output,
                             kOutputBus,
                             &zero, sizeof(zero));
    if (s != noErr) { AudioComponentInstanceDispose(impl_->unit); return map_osstatus(s); }

#if !TARGET_OS_IPHONE
    // CRITICAL (macOS): a HALOutput unit binds to the system's default OUTPUT
    // device by default, which has no input stream — so AudioUnitRender in the
    // input callback would return silence/no frames. Point the unit at the
    // default INPUT device explicitly. iOS RemoteIO always uses the system
    // input, so this step is macOS-only.
    {
        AudioDeviceID inputDevice = kAudioObjectUnknown;
        UInt32 devSize = sizeof(inputDevice);
        AudioObjectPropertyAddress addr = {
            kAudioHardwarePropertyDefaultInputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        s = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr,
                                       0, nullptr, &devSize, &inputDevice);
        if (s != noErr || inputDevice == kAudioObjectUnknown) {
            AudioComponentInstanceDispose(impl_->unit);
            return (s != noErr) ? map_osstatus(s) : AURIS_ERR_BACKEND;
        }
        s = AudioUnitSetProperty(impl_->unit,
                                 kAudioOutputUnitProperty_CurrentDevice,
                                 kAudioUnitScope_Global,
                                 kOutputBus,
                                 &inputDevice, sizeof(inputDevice));
        if (s != noErr) { AudioComponentInstanceDispose(impl_->unit); return map_osstatus(s); }
    }
#endif

    AudioStreamBasicDescription fmt{};
    fmt.mSampleRate       = cfg.sample_rate_hz;
    fmt.mFormatID         = kAudioFormatLinearPCM;
    fmt.mFormatFlags      = kAudioFormatFlagIsSignedInteger |
                            kAudioFormatFlagIsPacked        |
                            kAudioFormatFlagsNativeEndian;
    fmt.mFramesPerPacket  = 1;
    fmt.mChannelsPerFrame = cfg.channels;
    fmt.mBitsPerChannel   = 16;
    fmt.mBytesPerFrame    = static_cast<UInt32>(cfg.channels) * 2u;
    fmt.mBytesPerPacket   = fmt.mBytesPerFrame;

    s = AudioUnitSetProperty(impl_->unit,
                             kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Output,    // Output of the input bus.
                             kInputBus,
                             &fmt, sizeof(fmt));
    if (s != noErr) { AudioComponentInstanceDispose(impl_->unit); return map_osstatus(s); }

    AURenderCallbackStruct cbStruct{};
    cbStruct.inputProc       = input_render_callback;
    cbStruct.inputProcRefCon = impl_.get();
    s = AudioUnitSetProperty(impl_->unit,
                             kAudioOutputUnitProperty_SetInputCallback,
                             kAudioUnitScope_Global,
                             kInputBus,
                             &cbStruct, sizeof(cbStruct));
    if (s != noErr) { AudioComponentInstanceDispose(impl_->unit); return map_osstatus(s); }

    s = AudioUnitInitialize(impl_->unit);
    if (s != noErr) { AudioComponentInstanceDispose(impl_->unit); return map_osstatus(s); }

    impl_->initialized = true;
    return AURIS_OK;
}

auris_status_t CoreAudioBackend::start() {
    if (!impl_->initialized) return AURIS_ERR_INVALID_STATE;
    if (impl_->running)      return AURIS_ERR_ALREADY_RUNNING;
    const OSStatus s = AudioOutputUnitStart(impl_->unit);
    if (s != noErr) return map_osstatus(s);
    impl_->running = true;
    return AURIS_OK;
}

auris_status_t CoreAudioBackend::stop() {
    if (!impl_->running) return AURIS_OK;
    AudioOutputUnitStop(impl_->unit);
    impl_->running = false;
#if TARGET_OS_IPHONE
    // Release the audio session so other apps regain the route.
    [[AVAudioSession sharedInstance]
        setActive:NO
      withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
            error:nil];
#endif
    return AURIS_OK;
}

} // namespace auris

#endif // __APPLE__
