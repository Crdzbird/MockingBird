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

  /// Audio kept after the detector closes, emitted as post-roll packets.
  final int postRollMs;

  /// Hard cap on one segment; longer segments are split. 0 is unbounded.
  final int maxMs;
}
