/// The queues between capture, the worker, and the host.
final class MockingbirdQueueConfig {
  /// Creates a queue configuration.
  const factory MockingbirdQueueConfig({
    int pcmRingFrames,
    int packetQueueCapacity,
    bool pcmTap,
    int pcmTapFrames,
  }) = MockingbirdQueueConfig._;

  const MockingbirdQueueConfig._({
    this.pcmRingFrames = 16384,
    this.packetQueueCapacity = 256,
    this.pcmTap = false,
    this.pcmTapFrames = 0,
  });

  /// Capacity of the capture-to-worker PCM ring, in samples.
  final int pcmRingFrames;

  /// Capacity of the worker-to-host packet queue; drop-oldest when full.
  final int packetQueueCapacity;

  /// Also expose every processed frame, ungated by segments, through
  /// `MockingbirdEngine.readPcm`, for waveforms or archiving the whole stream.
  final bool pcmTap;

  /// Tap ring capacity in samples; 0 is one second at the working rate.
  final int pcmTapFrames;
}
