/// How detections are cut into segments of audio.
final class MockingbirdSegmentConfig {
  /// Creates a segmentation configuration.
  const factory MockingbirdSegmentConfig({
    int preRollMs,
    int postRollMs,
    int maxMs,
  }) = MockingbirdSegmentConfig._;

  const MockingbirdSegmentConfig._({
    this.preRollMs = 300,
    this.postRollMs = 300,
    this.maxMs = 60000,
  });

  /// Audio kept from before the trigger, emitted as pre-roll packets.
  final int preRollMs;

  /// Audio kept after the detectors go quiet, emitted as post-roll packets.
  /// The segment closes after the longer of this and the VAD hangover.
  final int postRollMs;

  /// Hard cap on one segment. The packet that reaches it is the last, and
  /// the next active frame opens a new segment. 0 is unbounded.
  final int maxMs;
}
