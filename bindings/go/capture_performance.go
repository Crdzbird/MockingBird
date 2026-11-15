package mockingbird

// CapturePerformance chooses how the platform runs live capture
// (mockingbird_config_t.capture_performance). Only Android (Oboe) and the
// miniaudio backend act on it; Core Audio and host PCM ignore it. It is not
// live-tunable: UpdateConfig refuses a change.
type CapturePerformance int32

const (
	// CaptureDefault is the platform's normal capture path. On Android it is
	// Oboe's PerformanceMode::None, which keeps real time while a camera
	// pipeline shares the audio DSP.
	CaptureDefault CapturePerformance = 0
	// CaptureLowLatency asks for the fast path (Oboe LowLatency, AAudio MMAP),
	// which can stall and drop audio when another heavy client shares it.
	CaptureLowLatency CapturePerformance = 1
	// CapturePowerSaving asks for large buffers (Oboe PowerSaving, miniaudio's
	// conservative profile).
	CapturePowerSaving CapturePerformance = 2
)
