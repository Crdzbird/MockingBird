/// The queues between capture, the worker, and the host.
final class AurisQueueConfig {
  /// Creates a queue configuration.
  const AurisQueueConfig({
    this.pcmRingFrames = 16384,
    this.packetQueueCapacity = 256,
    this.pcmTap = false,
    this.pcmTapFrames = 0,
  });

  /// Capacity of the capture → worker PCM ring, in samples.
  final int pcmRingFrames;

  /// Capacity of the worker → host packet queue; drop-oldest when full.
  final int packetQueueCapacity;

  /// Also expose every processed frame, ungated by segments, through
  /// `AurisEngine.readPcm` — for waveforms or archiving the whole stream.
  final bool pcmTap;

  /// Tap ring capacity in samples; 0 is one second at the working rate.
  final int pcmTapFrames;
}
