// One AVAudioSession per process, shared by capture and playback on iOS. Its
// category follows who holds it: Record for capture, Playback for a player,
// PlayAndRecord when both run or voice processing is on. A no-op on macOS.
#pragma once

#include "mockingbird/mockingbird.h"

#include <cstdint>

namespace mockingbird {
enum class SessionUse { Capture, VoiceCapture, Playback };

mockingbird_status_t audio_session_acquire(SessionUse use, uint32_t preferred_rate_hz);
void audio_session_release(SessionUse use);

}
