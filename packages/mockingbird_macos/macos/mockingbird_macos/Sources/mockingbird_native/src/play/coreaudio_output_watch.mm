// What the OS does to Apple playback. iOS: an interruption stops the unit and
// its end restarts it when iOS says to resume; the route the player used going
// away (headphones unplugged) pauses it until the host resumes, as iOS apps
// are expected to; a new route is followed. macOS needs nothing: the default
// output unit moves to the new default device by itself.
#include "play/coreaudio_output.hpp"

#if defined(__APPLE__)

#include "play/coreaudio_output_impl.hpp"

#if TARGET_OS_IPHONE
#import <AVFoundation/AVFoundation.h>
#endif

namespace mockingbird {
namespace {
#if TARGET_OS_IPHONE
using ImplPtr = std::shared_ptr<CoreAudioOutput::Impl>;

void* observe(NSNotificationName name, void (^handler)(NSNotification*)) {
    id token = [[NSNotificationCenter defaultCenter] addObserverForName:name object:nil
                                                                  queue:nil usingBlock:handler];
    return (void*)CFBridgingRetain(token);
}

void interrupted(const ImplPtr& p, NSNotification* n) {
    std::lock_guard lk(p->mu);
    if (!p->alive || !p->wanted) return;
    const NSUInteger type = [n.userInfo[AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
    if (type == AVAudioSessionInterruptionTypeBegan) {
        p->running = false;
        p->tell(OutputNotice::Interrupted);
        return;
    }
    const NSUInteger options = [n.userInfo[AVAudioSessionInterruptionOptionKey] unsignedIntegerValue];
    if ((options & AVAudioSessionInterruptionOptionShouldResume) == 0) return;
    [[AVAudioSession sharedInstance] setActive:YES error:nil];
    if (p->start_unit()) p->tell(OutputNotice::Resumed);
}

void rerouted(const ImplPtr& p, NSNotification* n) {
    std::lock_guard lk(p->mu);
    if (!p->alive || !p->wanted) return;
    const NSUInteger reason = [n.userInfo[AVAudioSessionRouteChangeReasonKey] unsignedIntegerValue];
    if (reason == AVAudioSessionRouteChangeReasonOldDeviceUnavailable && p->running) {
        p->stop_unit();
        p->tell(OutputNotice::RouteLost);
        return;
    }
    if (reason == AVAudioSessionRouteChangeReasonNewDeviceAvailable) p->tell(OutputNotice::RouteChanged);
}
#endif

}

void coreaudio_output_watch(const std::shared_ptr<CoreAudioOutput::Impl>& impl) {
#if TARGET_OS_IPHONE
    std::shared_ptr<CoreAudioOutput::Impl> p = impl;
    impl->observers[0] = observe(AVAudioSessionInterruptionNotification, ^(NSNotification* n) {
        interrupted(p, n);
    });
    impl->observers[1] = observe(AVAudioSessionRouteChangeNotification, ^(NSNotification* n) {
        rerouted(p, n);
    });
    impl->observers[2] = observe(AVAudioSessionMediaServicesWereResetNotification, ^(NSNotification*) {
        std::lock_guard lk(p->mu);
        if (p->alive && p->wanted) p->tell(OutputNotice::Interrupted);
    });
#else
    (void)impl;
#endif
}

void coreaudio_output_unwatch(CoreAudioOutput::Impl& impl) {
#if TARGET_OS_IPHONE
    for (void*& token : impl.observers) {
        if (token == nullptr) continue;
        [[NSNotificationCenter defaultCenter] removeObserver:(id)CFBridgingRelease(token)];
        token = nullptr;
    }
#else
    (void)impl;
#endif
}

}

#endif
