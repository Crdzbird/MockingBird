import 'package:auris/auris.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:flutter_test/flutter_test.dart';

AurisEvent decode(int type, {int reason = 0, double confidence = 0}) =>
    AurisEvent.fromNative(
      AurisNativeEvent(
        type: type,
        segmentId: 7,
        timestampUs: 123456,
        reasonMask: reason,
        confidence: confidence,
      ),
    );

void main() {
  test('every native type decodes to its sealed subtype', () {
    expect(decode(1, confidence: 0.9), isA<AurisSpeechBeginEvent>());
    expect(decode(2), isA<AurisSpeechEndEvent>());
    expect(decode(3, reason: 3), isA<AurisAnomalyEvent>());
    expect(decode(4), isA<AurisSegmentBeginEvent>());
    expect(decode(5), isA<AurisSegmentEndEvent>());
    expect(decode(6), isA<AurisOverrunEvent>());
    expect(decode(7), isA<AurisBackendErrorEvent>());
    expect(decode(8, reason: 70), isA<AurisSoundClassEvent>());
    expect(decode(9, reason: 2), isA<AurisDegradationEvent>());
    expect(decode(42), isA<AurisUnknownEvent>());
  });

  test('common fields and typed payloads survive decoding', () {
    final e = decode(8, reason: 70, confidence: 0.75) as AurisSoundClassEvent;
    expect(e.segmentId, 7);
    expect(e.timestampUs, 123456);
    expect(e.classId, 70);
    expect(e.category, AurisSoundCategory.animal);
    expect(e.isHumanVoice, isFalse);
    expect(e.confidence, closeTo(0.75, 1e-6));
  });

  test('anomaly reasons unpack from the mask', () {
    final e = decode(3, reason: 5) as AurisAnomalyEvent;
    expect(e.reasons, {AurisAnomalyReason.rms, AurisAnomalyReason.centroid});
  });

  test('a backend error carries the negative status through a uint32', () {
    final e = decode(7, reason: 0xFFFFFFFC) as AurisBackendErrorEvent;
    expect(e.status, AurisStatus.backend);
  });

  test('degradation reports level and busy fraction', () {
    final e = decode(9, reason: 2, confidence: 0.83) as AurisDegradationEvent;
    expect(e.level, 2);
    expect(e.busyFraction, closeTo(0.83, 1e-6));
  });
}
