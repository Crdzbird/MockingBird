package mockingbird

// EventType identifies what an Event reports.
type EventType int

const (
	EventSpeechBegin  EventType = 1
	EventSpeechEnd    EventType = 2
	EventAnomaly      EventType = 3
	EventSegmentBegin EventType = 4
	EventSegmentEnd   EventType = 5
	EventOverrun      EventType = 6
	EventBackendError EventType = 7
	EventSoundClass   EventType = 8
	EventDegradation  EventType = 9
)

// String returns the event type's name.
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
	case EventDegradation:
		return "DEGRADATION"
	}
	return "UNKNOWN"
}
