package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

// CaptureHealth says what the capture backend is doing for the microphone.
type CaptureHealth struct {
	// VoiceProcessing is what the platform applied, which can be less than
	// asked for.
	VoiceProcessing VoiceProcessing
	// Restarts counts returns after an interruption or a route change.
	Restarts uint32
	// Gaps counts times the platform delivered no audio for more than
	// 200 ms beyond what it had delivered: a stalled DSP, a late callback, an
	// interruption. GapMs is their total length. Zero for host PCM.
	Gaps  uint32
	GapMs uint32
}

func captureHealthFromC(s C.mockingbird_stats_t) CaptureHealth {
	return CaptureHealth{
		VoiceProcessing: VoiceProcessing(s.voice_processing_active),
		Restarts:        uint32(s.capture_restarts),
		Gaps:            uint32(s.capture_gaps),
		GapMs:           uint32(s.capture_gap_ms),
	}
}
