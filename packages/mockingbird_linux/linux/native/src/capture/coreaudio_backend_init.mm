// Opens and configures the capture unit. iOS requires an active recording
// AVAudioSession before the unit will initialise; voice processing uses the
// play-and-record category in voice-chat mode, as VoiceProcessingIO expects.
// VoiceProcessingIO only initialises with its output side live, so in that
// mode the unit keeps output enabled and renders silence into it.
#include "capture/coreaudio_backend.hpp"

#if defined(__APPLE__)

#include "capture/coreaudio_impl.hpp"

#if TARGET_OS_IPHONE
#import <AVFoundation/AVFoundation.h>
#endif

namespace mockingbird {
namespace {
mockingbird_status_t activate_session(const mockingbird_config_t& cfg) {
#if TARGET_OS_IPHONE
    AVAudioSession* session = [AVAudioSession sharedInstance];
    NSError* err = nil;
    const bool voice = cfg.voice_processing != 0;
    const BOOL set = voice
        ? [session setCategory:AVAudioSessionCategoryPlayAndRecord
                          mode:AVAudioSessionModeVoiceChat
                       options:0
                         error:&err]
        : [session setCategory:AVAudioSessionCategoryRecord error:&err];
    if (!set) return MOCKINGBIRD_ERR_BACKEND;
    [session setPreferredSampleRate:static_cast<double>(cfg.sample_rate_hz) error:nil];
    if (![session setActive:YES error:&err]) return MOCKINGBIRD_ERR_BACKEND;
#else
    (void)cfg;
#endif
    return MOCKINGBIRD_OK;
}

OSStatus set_stream_format(AudioComponentInstance unit, const mockingbird_config_t& cfg, bool voice) {
    AudioStreamBasicDescription fmt{};
    fmt.mSampleRate       = cfg.sample_rate_hz;
    fmt.mFormatID         = kAudioFormatLinearPCM;
    fmt.mFormatFlags      = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked |
                            kAudioFormatFlagsNativeEndian;
    fmt.mFramesPerPacket  = 1;
    fmt.mChannelsPerFrame = cfg.channels;
    fmt.mBitsPerChannel   = 16;
    fmt.mBytesPerFrame    = static_cast<UInt32>(cfg.channels) * 2u;
    fmt.mBytesPerPacket   = fmt.mBytesPerFrame;
    const OSStatus s = AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat,
                                            kAudioUnitScope_Output, kCoreAudioInputBus, &fmt,
                                            sizeof(fmt));
    if (s != noErr || !voice) return s;
    return coreaudio_play_silence(unit, fmt);
}

}

mockingbird_status_t coreaudio_open_unit(CoreAudioBackend::Impl& impl, const mockingbird_config_t& cfg) {
    const mockingbird_status_t session = activate_session(cfg);
    if (session != MOCKINGBIRD_OK) return session;
    impl.voice_requested = cfg.voice_processing;
    impl.unit = coreaudio_create_unit(impl.voice_requested != 0);
    if (impl.unit == nullptr) return MOCKINGBIRD_ERR_BACKEND;
    const bool voice = impl.voice_requested != 0;
    OSStatus s = voice ? noErr : coreaudio_enable_input_only(impl.unit);
    if (s == noErr && (!voice || !impl.device_id.empty())) {
        s = coreaudio_select_input(impl.unit, impl.device_id,
                                   voice ? kCoreAudioInputBus : kCoreAudioOutputBus);
        if (s != noErr && !impl.device_id.empty()) {
            AudioComponentInstanceDispose(impl.unit);
            impl.unit = nullptr;
            return MOCKINGBIRD_ERR_INVALID_ARG;
        }
    }
    if (s == noErr) s = set_stream_format(impl.unit, cfg, voice);
    if (s != noErr) {
        AudioComponentInstanceDispose(impl.unit);
        impl.unit = nullptr;
        return MOCKINGBIRD_ERR_BACKEND;
    }
    impl.voice_active = coreaudio_configure_voice(impl.unit, impl.voice_requested);
    return MOCKINGBIRD_OK;
}

}

#endif
