// Opens and configures the capture unit. iOS requires an active recording
// AVAudioSession before the unit will initialise.
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
    if (![session setCategory:AVAudioSessionCategoryRecord error:&err]) return MOCKINGBIRD_ERR_BACKEND;
    [session setPreferredSampleRate:static_cast<double>(cfg.sample_rate_hz) error:nil];
    if (![session setActive:YES error:&err]) return MOCKINGBIRD_ERR_BACKEND;
#else
    (void)cfg;
#endif
    return MOCKINGBIRD_OK;
}

OSStatus set_stream_format(AudioComponentInstance unit, const mockingbird_config_t& cfg) {
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
    return AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output,
                                kCoreAudioInputBus, &fmt, sizeof(fmt));
}

} // namespace

mockingbird_status_t coreaudio_open_unit(CoreAudioBackend::Impl& impl, const mockingbird_config_t& cfg) {
    const mockingbird_status_t session = activate_session(cfg);
    if (session != MOCKINGBIRD_OK) return session;
    impl.unit = coreaudio_create_unit();
    if (impl.unit == nullptr) return MOCKINGBIRD_ERR_BACKEND;
    OSStatus s = coreaudio_enable_input_only(impl.unit);
    if (s == noErr) s = coreaudio_select_default_input(impl.unit);
    if (s == noErr) s = set_stream_format(impl.unit, cfg);
    if (s != noErr) {
        AudioComponentInstanceDispose(impl.unit);
        impl.unit = nullptr;
        return MOCKINGBIRD_ERR_BACKEND;
    }
    return MOCKINGBIRD_OK;
}

} // namespace mockingbird

#endif
