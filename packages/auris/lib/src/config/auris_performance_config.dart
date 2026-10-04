/// Knobs that trade latency for CPU and battery on constrained devices.
final class AurisPerformanceConfig {
  /// Creates a performance configuration.
  const AurisPerformanceConfig({
    this.silenceGateDbfs = -60,
    this.adaptiveDegradation = false,
    this.degradeBusyPpm = 700000,
    this.workerBatchFrames = 0,
  });

  /// Tuned for a live microphone on a phone: shed YAMNet work under load
  /// and wake the worker every four frames instead of every frame.
  const AurisPerformanceConfig.mobile()
    : this(adaptiveDegradation: true, workerBatchFrames: 4);

  /// Frames below this input level (dBFS) skip RNNoise, and YAMNet skips an
  /// inference whose whole window was quiet. VAD and anomaly are never
  /// gated; audio always flows. -60 is conservative, -50 also sheds
  /// quiet-room tone, -120 or lower disables. Live-tunable.
  final double silenceGateDbfs;

  /// Shed YAMNet work (cadence ×2, ×4, then suspend) when the worker exceeds
  /// [degradeBusyPpm] or drops frames, restoring it after a few calm
  /// seconds. Emits `AurisDegradationEvent`. Off by default because it
  /// reacts to wall-clock timing, which offline processing must not.
  final bool adaptiveDegradation;

  /// Busy threshold for degradation, parts per million.
  final int degradeBusyPpm;

  /// Wake the native worker only once this many frames are queued: the same
  /// CPU work in N× fewer wakeups, for up to N−1 frames of latency.
  final int workerBatchFrames;
}
