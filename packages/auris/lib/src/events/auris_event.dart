import 'package:auris/src/auris_sound_category.dart';
import 'package:auris/src/auris_status.dart';
import 'package:auris/src/events/auris_anomaly_reason.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:flutter/foundation.dart' show objectRuntimeType;

part 'auris_anomaly_event.dart';
part 'auris_backend_error_event.dart';
part 'auris_degradation_event.dart';
part 'auris_overrun_event.dart';
part 'auris_segment_begin_event.dart';
part 'auris_segment_end_event.dart';
part 'auris_sound_class_event.dart';
part 'auris_speech_begin_event.dart';
part 'auris_speech_end_event.dart';
part 'auris_unknown_event.dart';

/// Something the engine observed. Switch on the subtype; the hierarchy is
/// sealed, so the switch is exhaustive.
sealed class AurisEvent {
  const AurisEvent({required this.segmentId, required this.timestampUs});

  /// Decodes one native `auris_event_t`.
  factory AurisEvent.fromNative(auris_event_t e) {
    final id = e.segment_id;
    final ts = e.timestamp_us;
    return switch (e.typeAsInt) {
      1 => AurisSpeechBeginEvent._(id, ts, e.confidence),
      2 => AurisSpeechEndEvent._(id, ts),
      3 => AurisAnomalyEvent._(id, ts, e.reason_mask, e.confidence),
      4 => AurisSegmentBeginEvent._(id, ts, e.confidence),
      5 => AurisSegmentEndEvent._(id, ts),
      6 => AurisOverrunEvent._(id, ts),
      7 => AurisBackendErrorEvent._(id, ts, e.reason_mask.toSigned(32)),
      8 => AurisSoundClassEvent._(id, ts, e.reason_mask, e.confidence),
      9 => AurisDegradationEvent._(id, ts, e.reason_mask, e.confidence),
      final other => AurisUnknownEvent._(id, ts, other),
    };
  }

  /// The segment the event belongs to, or 0.
  final int segmentId;

  /// Capture-clock timestamp, microseconds.
  final int timestampUs;

  @override
  String toString() =>
      '${objectRuntimeType(this, 'AurisEvent')}'
      '(segment=$segmentId at $timestampUs us)';
}
