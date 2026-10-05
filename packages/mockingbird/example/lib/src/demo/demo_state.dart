import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_example/src/demo/demo_preset.dart';
import 'package:mockingbird_example/src/demo/demo_presets.dart';
import 'package:mockingbird_example/src/demo/detection_summary.dart';
import 'package:mockingbird_example/src/demo/event_formatter.dart';
import 'package:equatable/equatable.dart';
import 'package:flutter/foundation.dart' show ValueGetter;

/// Everything the demo screen renders.
final class DemoState extends Equatable {
  const DemoState({
    this.running = false,
    this.presetIndex = 0,
    this.sensitivity = 0.6,
    this.denoise = false,
    this.error,
    this.stats,
    this.packetCount = 0,
    this.packetBytes = 0,
    this.detection = const DetectionSummary(),
    this.log = const [],
  });

  final bool running;
  final int presetIndex;
  final double sensitivity;
  final bool denoise;
  final String? error;
  final AurisStats? stats;
  final int packetCount;
  final int packetBytes;
  final DetectionSummary detection;
  final List<String> log;

  static const _logMaxLines = 200;

  DemoPreset get preset => demoPresets[presetIndex];

  DemoState withEvent(AurisEvent event, EventFormatter formatter) => copyWith(
    detection: detection.applyEvent(event, formatter),
    log: [formatter.format(event), ...log.take(_logMaxLines - 1)],
  );

  DemoState withPacket(AurisPacket packet) => copyWith(
    packetCount: packetCount + 1,
    packetBytes: packetBytes + packet.data.length,
  );

  DemoState copyWith({
    bool? running,
    int? presetIndex,
    double? sensitivity,
    bool? denoise,
    ValueGetter<String?>? error,
    AurisStats? stats,
    int? packetCount,
    int? packetBytes,
    DetectionSummary? detection,
    List<String>? log,
  }) => DemoState(
    running: running ?? this.running,
    presetIndex: presetIndex ?? this.presetIndex,
    sensitivity: sensitivity ?? this.sensitivity,
    denoise: denoise ?? this.denoise,
    error: error != null ? error() : this.error,
    stats: stats ?? this.stats,
    packetCount: packetCount ?? this.packetCount,
    packetBytes: packetBytes ?? this.packetBytes,
    detection: detection ?? this.detection,
    log: log ?? this.log,
  );

  @override
  List<Object?> get props => [
    running,
    presetIndex,
    sensitivity,
    denoise,
    error,
    stats,
    packetCount,
    packetBytes,
    detection,
    log,
  ];
}
