package mockingbird

/*
#include "mockingbird/mockingbird.h"
*/
import "C"

// Stats is a snapshot of the engine's counters. It never carries audio.
type Stats struct {
	FramesCaptured       uint64
	FramesDroppedCapture uint64
	PacketsEmitted       uint64
	PacketsDropped       uint64
	SegmentsOpened       uint64
	SegmentsClosed       uint64
	PcmRingFillFrames    uint32
	PacketQueueFill      uint32
	// InputRMSdBFS is the latest frame's level before denoising; -120 is
	// digital silence. Drive a level meter with it.
	InputRMSdBFS  float32
	InputPeakdBFS float32
	// TapSamplesDropped counts PCM-tap samples the host did not drain.
	TapSamplesDropped uint64
	// Worker is the worker thread's health and work shedding.
	Worker WorkerHealth
	// Capture is the voice processing in effect and the capture restarts.
	Capture CaptureHealth
}

func statsFromC(s C.mockingbird_stats_t) Stats {
	return Stats{
		FramesCaptured:       uint64(s.frames_captured),
		FramesDroppedCapture: uint64(s.frames_dropped_capture),
		PacketsEmitted:       uint64(s.packets_emitted),
		PacketsDropped:       uint64(s.packets_dropped),
		SegmentsOpened:       uint64(s.segments_opened),
		SegmentsClosed:       uint64(s.segments_closed),
		PcmRingFillFrames:    uint32(s.pcm_ring_fill_frames),
		PacketQueueFill:      uint32(s.packet_queue_fill),
		InputRMSdBFS:         float32(s.input_rms_dbfs),
		InputPeakdBFS:        float32(s.input_peak_dbfs),
		TapSamplesDropped:    uint64(s.tap_samples_dropped),
		Worker:               workerHealthFromC(s),
		Capture:              captureHealthFromC(s),
	}
}
