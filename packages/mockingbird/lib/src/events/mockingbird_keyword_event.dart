part of 'mockingbird_event.dart';

/// A keyword added with `MockingbirdEngine.addKeyword` was heard.
final class MockingbirdKeywordEvent extends MockingbirdEvent {
  const MockingbirdKeywordEvent._(
    int segmentId,
    int timestampUs,
    this.keywordId,
    this.score,
  ) : super(segmentId: segmentId, timestampUs: timestampUs);

  /// The id `addKeyword` returned for the keyword.
  final int keywordId;

  /// The keyword model's score, 0..1, at or above its threshold.
  final double score;
}
