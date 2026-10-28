// Keeps capture alive across what the OS does to it. On macOS, an engine
// that follows the default input moves to the new default when it changes.
// On iOS, session interruptions (a call, Siri, another app recording) pause
// capture and the unit restarts when they end; route changes are reported.
#include "capture/coreaudio_backend.hpp"

#if defined(__APPLE__)

#include "capture/coreaudio_impl.hpp"

#if TARGET_OS_IPHONE
#import <AVFoundation/AVFoundation.h>
#else
#include <CoreAudio/AudioHardware.h>
#endif

namespace mockingbird {
namespace {
#if !TARGET_OS_IPHONE
const AudioObjectPropertyAddress kDefaultInput = {kAudioHardwarePropertyDefaultInputDevice,
                                                  kAudioObjectPropertyScopeGlobal,
                                                  kAudioObjectPropertyElementMain};

OSStatus default_input_changed(AudioObjectID, UInt32, const AudioObjectPropertyAddress*,
                               void* client) {
    auto* impl = static_cast<CoreAudioBackend::Impl*>(client);
    std::lock_guard lk(impl->route_mu);
    if (!impl->running || !impl->device_id.empty()) return noErr;
    AudioOutputUnitStop(impl->unit);
    AudioUnitUninitialize(impl->unit);
    if (impl->voice_requested == 0) {
        coreaudio_select_input(impl->unit, std::string(), kCoreAudioOutputBus);
    }
    const bool ok = AudioUnitInitialize(impl->unit) == noErr && AudioOutputUnitStart(impl->unit) == noErr;
    impl->tell(ok ? MOCKINGBIRD_EVT_ROUTE_CHANGED : MOCKINGBIRD_EVT_CAPTURE_INTERRUPTED);
    return noErr;
}
#else
void* observe(NSNotificationName name, void (^handler)(NSNotification*)) {
    id token = [[NSNotificationCenter defaultCenter] addObserverForName:name object:nil
                                                                  queue:nil usingBlock:handler];
    return (void*)CFBridgingRetain(token);
}
#endif

}

void coreaudio_watch_routes(CoreAudioBackend::Impl& impl) {
    if (impl.watching) return;
    impl.watching = true;
#if !TARGET_OS_IPHONE
    AudioObjectAddPropertyListener(kAudioObjectSystemObject, &kDefaultInput, default_input_changed, &impl);
#else
    CoreAudioBackend::Impl* p = &impl;
    impl.observers[0] = observe(AVAudioSessionInterruptionNotification, ^(NSNotification* n) {
        const NSUInteger type = [n.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
        if (type == AVAudioSessionInterruptionTypeBegan) {
            p->tell(MOCKINGBIRD_EVT_CAPTURE_INTERRUPTED);
            return;
        }
        std::lock_guard lk(p->route_mu);
        if (!p->running) return;
        [[AVAudioSession sharedInstance] setActive:YES error:nil];
        const bool ok = AudioOutputUnitStart(p->unit) == noErr;
        p->tell(ok ? MOCKINGBIRD_EVT_CAPTURE_RESUMED : MOCKINGBIRD_EVT_CAPTURE_INTERRUPTED);
    });
    impl.observers[1] = observe(AVAudioSessionRouteChangeNotification, ^(NSNotification* n) {
        const NSUInteger reason = [n.userInfo[AVAudioSessionRouteChangeReasonKey] unsignedIntegerValue];
        if (reason == AVAudioSessionRouteChangeReasonNewDeviceAvailable ||
            reason == AVAudioSessionRouteChangeReasonOldDeviceUnavailable) {
            p->tell(MOCKINGBIRD_EVT_ROUTE_CHANGED);
        }
    });
    impl.observers[2] = observe(AVAudioSessionMediaServicesWereResetNotification, ^(NSNotification*) {
        p->tell(MOCKINGBIRD_EVT_CAPTURE_INTERRUPTED);
    });
#endif
}

void coreaudio_unwatch_routes(CoreAudioBackend::Impl& impl) {
    if (!impl.watching) return;
    impl.watching = false;
#if !TARGET_OS_IPHONE
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &kDefaultInput, default_input_changed, &impl);
#else
    for (void*& token : impl.observers) {
        if (token == nullptr) continue;
        [[NSNotificationCenter defaultCenter] removeObserver:(id)CFBridgingRelease(token)];
        token = nullptr;
    }
#endif
}

}

#endif
