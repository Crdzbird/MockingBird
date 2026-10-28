import 'package:flutter/foundation.dart' show objectRuntimeType;
import 'package:mockingbird/src/events/mockingbird_anomaly_reason.dart';
import 'package:mockingbird/src/mockingbird_sound_category.dart';
import 'package:mockingbird/src/mockingbird_status.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

part 'mockingbird_anomaly_event.dart';
part 'mockingbird_backend_error_event.dart';
part 'mockingbird_capture_interrupted_event.dart';
part 'mockingbird_capture_resumed_event.dart';
part 'mockingbird_degradation_event.dart';
part 'mockingbird_keyword_event.dart';
part 'mockingbird_overrun_event.dart';
part 'mockingbird_route_changed_event.dart';
part 'mockingbird_segment_begin_event.dart';
part 'mockingbird_segment_end_event.dart';
part 'mockingbird_speaker_event.dart';
part 'mockingbird_sound_class_event.dart';
part 'mockingbird_speech_begin_event.dart';
part 'mockingbird_speech_end_event.dart';
part 'mockingbird_unknown_event.dart';

/// Something the engine observed. Switch on the subtype; the hierarchy is
/// sealed, so the switch is exhaustive.
sealed class MockingbirdEvent {
  const MockingbirdEvent({required this.segmentId, required this.timestampUs});

  /// Decodes one native `mockingbird_event_t`.
  factory MockingbirdEvent.fromNative(MockingbirdNativeEvent e) {
    final id = e.segmentId;
    final ts = e.timestampUs;
    return switch (e.type) {
      1 => MockingbirdSpeechBeginEvent._(id, ts, e.confidence),
      2 => MockingbirdSpeechEndEvent._(id, ts),
      3 => MockingbirdAnomalyEvent._(id, ts, e.reasonMask, e.confidence),
      4 => MockingbirdSegmentBeginEvent._(id, ts, e.confidence),
      5 => MockingbirdSegmentEndEvent._(id, ts),
      6 => MockingbirdOverrunEvent._(id, ts),
      7 => MockingbirdBackendErrorEvent._(id, ts, e.reasonMask.toSigned(32)),
      8 => MockingbirdSoundClassEvent._(id, ts, e.reasonMask, e.confidence),
      9 => MockingbirdDegradationEvent._(id, ts, e.reasonMask, e.confidence),
      10 => MockingbirdCaptureInterruptedEvent._(id, ts),
      11 => MockingbirdCaptureResumedEvent._(id, ts),
      12 => MockingbirdRouteChangedEvent._(id, ts),
      13 => MockingbirdKeywordEvent._(id, ts, e.reasonMask, e.confidence),
      14 => MockingbirdSpeakerEvent._(id, ts, e.reasonMask == 1, e.confidence),
      final other => MockingbirdUnknownEvent._(id, ts, other),
    };
  }

  /// The segment the event belongs to, or 0.
  final int segmentId;

  /// Capture-clock timestamp, microseconds.
  final int timestampUs;

  @override
  String toString() =>
      '${objectRuntimeType(this, 'MockingbirdEvent')}'
      '(segment=$segmentId at $timestampUs us)';
}
