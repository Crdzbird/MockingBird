package auris

/*
#include "auris/auris.h"
*/
import "C"

// Event is something the engine observed. The typed accessors interpret
// ReasonMask and Confidence for the event types that carry a payload.
type Event struct {
	Type        EventType
	SegmentID   uint64
	TimestampUs uint64
	ReasonMask  uint32
	Confidence  float32
}

// SoundClass is the YAMNet AudioSet class of an EventSoundClass.
func (e Event) SoundClass() int { return int(e.ReasonMask) }

// Category is the broad category of an EventSoundClass.
func (e Event) Category() SoundCategory { return SoundCategoryOf(e.SoundClass()) }

// IsHumanVoice reports whether an EventSoundClass heard a person's voice.
func (e Event) IsHumanVoice() bool { return IsHumanVoice(e.SoundClass()) }

// AnomalyReasons are the statistics that tripped an EventAnomaly.
func (e Event) AnomalyReasons() AnomalyReason { return AnomalyReason(e.ReasonMask) }

// DegradationLevel is the new level of an EventDegradation, 0..3.
func (e Event) DegradationLevel() int { return int(e.ReasonMask) }

// BusyFraction is the worker's busy share that triggered an EventDegradation.
func (e Event) BusyFraction() float64 { return float64(e.Confidence) }

// BackendError is the status carried by an EventBackendError.
func (e Event) BackendError() Status { return Status(int32(e.ReasonMask)) }

func eventFromC(e C.auris_event_t) Event {
	return Event{
		Type:        EventType(e._type),
		SegmentID:   uint64(e.segment_id),
		TimestampUs: uint64(e.timestamp_us),
		ReasonMask:  uint32(e.reason_mask),
		Confidence:  float32(e.confidence),
	}
}
