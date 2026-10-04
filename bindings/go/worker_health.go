package auris

/*
#include "auris/auris.h"
*/
import "C"

// WorkerHealth describes how hard the native worker thread is working and
// what it is shedding.
type WorkerHealth struct {
	// BusyPpm is the share of wall time spent processing over the last ~1 s,
	// in parts per million; 1_000_000 is saturated.
	BusyPpm uint32
	// MaxFrameUs is the longest single-frame processing time since start.
	MaxFrameUs uint32
	// Wakeups counts thread parks and unparks, a battery proxy on phones.
	Wakeups uint64
	// SoundClassInferences counts YAMNet inferences that ran.
	SoundClassInferences uint64
	// SoundClassInferencesSkipped counts inferences shed by the silence gate
	// or degradation.
	SoundClassInferencesSkipped uint64
	// PreprocFramesSkipped counts frames RNNoise skipped as quiet.
	PreprocFramesSkipped uint64
	// DegradationLevel is 0 nominal through 3 classifier suspended.
	DegradationLevel uint32
}

// BusyFraction is BusyPpm as 0..1.
func (w WorkerHealth) BusyFraction() float64 { return float64(w.BusyPpm) / 1e6 }

func workerHealthFromC(s C.auris_stats_t) WorkerHealth {
	return WorkerHealth{
		BusyPpm:                     uint32(s.worker_busy_ppm),
		MaxFrameUs:                  uint32(s.worker_max_frame_us),
		Wakeups:                     uint64(s.worker_wakeups),
		SoundClassInferences:        uint64(s.sound_class_inferences),
		SoundClassInferencesSkipped: uint64(s.sound_class_inferences_skipped),
		PreprocFramesSkipped:        uint64(s.preproc_frames_skipped),
		DegradationLevel:            uint32(s.degradation_level),
	}
}
