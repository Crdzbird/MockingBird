/// How the platform runs live capture. Android (Oboe) and the miniaudio
/// backend act on it; Core Audio, the web and host PCM ignore it. Fixed for
/// the engine's life: `updateConfig` refuses a change.
enum MockingbirdCapturePerformance {
  /// The platform's normal capture path (`..._PERFORMANCE_DEFAULT`). On
  /// Android it is Oboe's `PerformanceMode::None`, which keeps real time
  /// while a camera pipeline shares the audio DSP.
  normal(0),

  /// The fast path (Oboe `LowLatency`, AAudio MMAP): lower latency, but it
  /// can stall and drop audio when another heavy client shares the device.
  lowLatency(1),

  /// Large buffers (Oboe `PowerSaving`, miniaudio's conservative profile).
  powerSaving(2);

  const MockingbirdCapturePerformance(this.nativeValue);

  /// The `mockingbird_capture_performance_t` value.
  final int nativeValue;
}
