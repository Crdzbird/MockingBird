// Counts the session's holders under one lock and sets the category for the
// set that is active. The session goes inactive, letting other apps resume
// their audio, only when the last holder releases it.
#include "platform/apple_audio_session.hpp"

#if defined(__APPLE__)

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#import <AVFoundation/AVFoundation.h>

#include <mutex>

namespace mockingbird {
namespace {
std::mutex g_mu;
uint32_t   g_held[3] = {0, 0, 0};

uint32_t& held(SessionUse use) { return g_held[static_cast<int>(use)]; }

bool apply_category() {
    AVAudioSession* session = [AVAudioSession sharedInstance];
    const bool capture  = held(SessionUse::Capture) > 0;
    const bool voice    = held(SessionUse::VoiceCapture) > 0;
    const bool playback = held(SessionUse::Playback) > 0;
    if (!capture && !voice) return [session setCategory:AVAudioSessionCategoryPlayback error:nil];
    if (!playback && !voice) return [session setCategory:AVAudioSessionCategoryRecord error:nil];
    const AVAudioSessionCategoryOptions options = playback
        ? (AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowBluetoothA2DP)
        : 0;
    return [session setCategory:AVAudioSessionCategoryPlayAndRecord
                           mode:voice ? AVAudioSessionModeVoiceChat : AVAudioSessionModeDefault
                        options:options
                          error:nil];
}

}

mockingbird_status_t audio_session_acquire(SessionUse use, uint32_t preferred_rate_hz) {
    std::lock_guard lk(g_mu);
    ++held(use);
    AVAudioSession* session = [AVAudioSession sharedInstance];
    if (!apply_category()) {
        --held(use);
        return MOCKINGBIRD_ERR_BACKEND;
    }
    if (preferred_rate_hz != 0) {
        [session setPreferredSampleRate:static_cast<double>(preferred_rate_hz) error:nil];
    }
    if (![session setActive:YES error:nil]) {
        --held(use);
        return MOCKINGBIRD_ERR_BACKEND;
    }
    return MOCKINGBIRD_OK;
}

void audio_session_release(SessionUse use) {
    std::lock_guard lk(g_mu);
    if (held(use) == 0) return;
    --held(use);
    AVAudioSession* session = [AVAudioSession sharedInstance];
    if (g_held[0] + g_held[1] + g_held[2] == 0) {
        [session setActive:NO
               withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                     error:nil];
        return;
    }
    apply_category();
}

}

#else

namespace mockingbird {
mockingbird_status_t audio_session_acquire(SessionUse, uint32_t) { return MOCKINGBIRD_OK; }
void audio_session_release(SessionUse) {}
}

#endif
#endif
