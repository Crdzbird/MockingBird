import 'package:flutter_test/flutter_test.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

MockingbirdEvent decode(int type, {int reason = 0, double confidence = 0}) =>
    MockingbirdEvent.fromNative(
      MockingbirdNativeEvent(
        type: type,
        segmentId: 7,
        timestampUs: 123456,
        reasonMask: reason,
        confidence: confidence,
      ),
    );

void main() {
  test('every native type decodes to its sealed subtype', () {
    expect(decode(1, confidence: 0.9), isA<MockingbirdSpeechBeginEvent>());
    expect(decode(2), isA<MockingbirdSpeechEndEvent>());
    expect(decode(3, reason: 3), isA<MockingbirdAnomalyEvent>());
    expect(decode(4), isA<MockingbirdSegmentBeginEvent>());
    expect(decode(5), isA<MockingbirdSegmentEndEvent>());
    expect(decode(6), isA<MockingbirdOverrunEvent>());
    expect(decode(7), isA<MockingbirdBackendErrorEvent>());
    expect(decode(8, reason: 70), isA<MockingbirdSoundClassEvent>());
    expect(decode(9, reason: 2), isA<MockingbirdDegradationEvent>());
    expect(decode(10), isA<MockingbirdCaptureInterruptedEvent>());
    expect(decode(11), isA<MockingbirdCaptureResumedEvent>());
    expect(decode(12), isA<MockingbirdRouteChangedEvent>());
    expect(decode(13, reason: 1), isA<MockingbirdKeywordEvent>());
    expect(decode(14, reason: 1), isA<MockingbirdSpeakerEvent>());
    expect(decode(42), isA<MockingbirdUnknownEvent>());
  });

  test('common fields and typed payloads survive decoding', () {
    final e =
        decode(8, reason: 70, confidence: 0.75) as MockingbirdSoundClassEvent;
    expect(e.segmentId, 7);
    expect(e.timestampUs, 123456);
    expect(e.classId, 70);
    expect(e.category, MockingbirdSoundCategory.animal);
    expect(e.isHumanVoice, isFalse);
    expect(e.confidence, closeTo(0.75, 1e-6));
  });

  test('anomaly reasons unpack from the mask', () {
    final e = decode(3, reason: 5) as MockingbirdAnomalyEvent;
    expect(e.reasons, {
      MockingbirdAnomalyReason.rms,
      MockingbirdAnomalyReason.centroid,
    });
  });

  test('a backend error carries the negative status through a uint32', () {
    final e = decode(7, reason: 0xFFFFFFFC) as MockingbirdBackendErrorEvent;
    expect(e.status, MockingbirdStatus.backend);
  });

  test('degradation reports level and busy fraction', () {
    final e =
        decode(9, reason: 2, confidence: 0.83) as MockingbirdDegradationEvent;
    expect(e.level, 2);
    expect(e.busyFraction, closeTo(0.83, 1e-6));
  });

  test('a keyword event carries its id and score', () {
    final e =
        decode(13, reason: 3, confidence: 0.92) as MockingbirdKeywordEvent;
    expect(e.keywordId, 3);
    expect(e.score, closeTo(0.92, 1e-6));
  });

  test('a speaker event says whether it matched and how closely', () {
    final e =
        decode(14, reason: 1, confidence: 0.81) as MockingbirdSpeakerEvent;
    expect(e.isTarget, isTrue);
    expect(e.similarity, closeTo(0.81, 1e-6));
    expect((decode(14) as MockingbirdSpeakerEvent).isTarget, isFalse);
  });
}
