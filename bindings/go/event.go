package auris

/*
#include "auris/auris.h"
*/
import "C"

import "unsafe"

// EventType mirrors auris_event_type_t.
type EventType int

const (
	EventSpeechBegin  EventType = 1
	EventSpeechEnd    EventType = 2
	EventAnomaly      EventType = 3 // non-speech acoustic event; ReasonMask = AnomalyReason bits
	EventSegmentBegin EventType = 4
	EventSegmentEnd   EventType = 5
	EventOverrun      EventType = 6 // packet queue dropped oldest
	EventBackendError EventType = 7 // ReasonMask carries an auris_status_t
	EventSoundClass   EventType = 8 // ReasonMask = YAMNet AudioSet class index (0..520)
)

// String returns a short name for the event type.
func (t EventType) String() string {
	switch t {
	case EventSpeechBegin:
		return "SPEECH_BEGIN"
	case EventSpeechEnd:
		return "SPEECH_END"
	case EventAnomaly:
		return "ANOMALY"
	case EventSegmentBegin:
		return "SEGMENT_BEGIN"
	case EventSegmentEnd:
		return "SEGMENT_END"
	case EventOverrun:
		return "OVERRUN"
	case EventBackendError:
		return "BACKEND_ERROR"
	case EventSoundClass:
		return "SOUND_CLASS"
	default:
		return "UNKNOWN"
	}
}

// Anomaly trigger bits, set in Event.ReasonMask when Type == EventAnomaly.
const (
	AnomalyRMS          uint32 = 1 << 0
	AnomalySpectralFlux uint32 = 1 << 1
	AnomalyCentroid     uint32 = 1 << 2
)

// Packet flag bits (Packet.Flags).
const (
	PacketFlagPreroll  uint32 = 1 << 0
	PacketFlagPostroll uint32 = 1 << 1
	PacketFlagKeyframe uint32 = 1 << 2
	PacketFlagLast     uint32 = 1 << 3
)

// Event is a detection event delivered on Engine.Events.
type Event struct {
	Type        EventType
	SegmentID   uint64 // 0 if not segment-bound
	TimestampUs uint64 // capture clock
	ReasonMask  uint32 // see per-type notes above
	Confidence  float32
}

// SoundClass returns the YAMNet AudioSet class index for an EventSoundClass
// event. Use CategoryForYAMNetClass / IsHumanVoice to interpret it.
func (e Event) SoundClass() int { return int(e.ReasonMask) }

// BackendError returns the Status carried by an EventBackendError event.
func (e Event) BackendError() Status { return Status(int32(e.ReasonMask)) }

// Packet is one encoded audio packet delivered on Engine.Packets. Data is a
// Go-owned copy — the native zero-copy borrow has already been released.
type Packet struct {
	SegmentID   uint64
	PtsUs       uint64 // offset from segment begin
	CaptureTsUs uint64
	Flags       uint32 // PacketFlag bits
	Codec       Codec
	Data        []byte
}

// Stats is an observability snapshot. It never carries audio bytes.
type Stats struct {
	FramesCaptured       uint64
	FramesDroppedCapture uint64 // SPSC ring overflow (should be 0)
	PacketsEmitted       uint64
	PacketsDropped       uint64 // bounded queue drop-oldest
	SegmentsOpened       uint64
	SegmentsClosed       uint64
	PcmRingFillFrames    uint32
	PacketQueueFill      uint32

	// Input level of the most recent frame, in dBFS (0 = full scale, negative
	// below, floored at -120 for digital silence). Measured before noise
	// suppression, so it reflects what the microphone received — drive a level
	// meter with this. Both are 0 until the first frame is processed.
	InputRMSdBFS  float32
	InputPeakdBFS float32

	// Samples the continuous PCM tap dropped because the host didn't drain it
	// fast enough. Always 0 when the tap is disabled.
	TapSamplesDropped uint64
}

func eventFromC(e C.auris_event_t) Event {
	return Event{
		Type:        EventType(e._type),
		SegmentID:   uint64(e.segment_id),
		TimestampUs: uint64(e.timestamp_us),
		ReasonMask:  uint32(e.reason_mask),
		Confidence:  float32(e.confidence),
	}
}

func packetFromC(p C.auris_packet_t) Packet {
	var data []byte
	if p.size > 0 && p.data != nil {
		data = C.GoBytes(unsafe.Pointer(p.data), C.int(p.size))
	}
	return Packet{
		SegmentID:   uint64(p.segment_id),
		PtsUs:       uint64(p.pts_us),
		CaptureTsUs: uint64(p.capture_ts_us),
		Flags:       uint32(p.flags),
		Codec:       Codec(p.codec),
		Data:        data,
	}
}

func statsFromC(s C.auris_stats_t) Stats {
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
	}
}
