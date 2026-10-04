package auris

// Backend selects where audio comes from.
type Backend int

const (
	// BackendAuto lets the engine pick the best native backend.
	BackendAuto Backend = iota
	// BackendMiniaudio is the portable capture backend.
	BackendMiniaudio
	// BackendOboe is Android's low-latency capture.
	BackendOboe
	// BackendCoreAudio is iOS and macOS capture with hardware timestamps.
	BackendCoreAudio
	// BackendHostPCM takes no microphone; the host feeds Engine.PushPCM.
	BackendHostPCM
)

// String names the backend.
func (b Backend) String() string {
	switch b {
	case BackendAuto:
		return "auto"
	case BackendMiniaudio:
		return "miniaudio"
	case BackendOboe:
		return "oboe"
	case BackendCoreAudio:
		return "coreaudio"
	case BackendHostPCM:
		return "host-pcm"
	}
	return "unknown"
}
