import 'dart:async';

import 'package:mockingbird/src/events/mockingbird_event.dart';
import 'package:mockingbird/src/mockingbird_packet.dart';
import 'package:mockingbird/src/mockingbird_segment.dart';
import 'package:mockingbird/src/mockingbird_stats.dart';
import 'package:mockingbird/src/native/segment_assembler.dart';

/// The engine's output streams. Segments are only assembled while someone
/// listens to [segments], so an app that never asks pays nothing for them.
final class EngineStreams {
  /// Creates the streams for an engine running at [sampleRateHz].
  factory EngineStreams({required int sampleRateHz}) = EngineStreams._;

  EngineStreams._({required int sampleRateHz}) {
    _assembler = SegmentAssembler(
      sampleRateHz: sampleRateHz,
      onSegment: _segments.add,
    );
  }

  /// Detections and lifecycle notices.
  final events = StreamController<MockingbirdEvent>.broadcast();

  /// Encoded packets.
  final packets = StreamController<MockingbirdPacket>.broadcast();

  /// Counter snapshots.
  final stats = StreamController<MockingbirdStats>.broadcast();

  final _segments = StreamController<MockingbirdSegment>.broadcast();
  late final SegmentAssembler _assembler;

  /// Finished segments.
  Stream<MockingbirdSegment> get segments => _segments.stream;

  /// Publishes one event.
  void onEvent(MockingbirdEvent e) {
    events.add(e);
    if (_segments.hasListener) _assembler.event(e);
  }

  /// Publishes one packet.
  void onPacket(MockingbirdPacket p) {
    packets.add(p);
    if (_segments.hasListener) _assembler.packet(p);
  }

  /// Hands over segments that ended during the drain just finished.
  void afterDrain() => _assembler.flush();

  /// Closes every stream.
  Future<void> close() async {
    await events.close();
    await packets.close();
    await stats.close();
    await _segments.close();
  }
}
