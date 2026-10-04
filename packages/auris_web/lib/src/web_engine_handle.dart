import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:auris_web/src/auris_module.dart';
import 'package:auris_web/src/web_capture.dart';

/// A WebAssembly engine pointer plus its scratch records and capture.
final class WebEngineHandle extends AurisEngineHandle {
  /// Allocates the scratch records for [pointer].
  WebEngineHandle(
    AurisModule m,
    this.pointer, {
    required this.captures,
    required this.sampleRateHz,
  })  : event = m.auris_web_event_new(),
        packet = m.auris_web_packet_new(),
        stats = m.auris_web_stats_new();

  /// The engine in linear memory.
  final int pointer;

  /// Whether the platform drives the microphone for this engine.
  final bool captures;

  /// The engine's sample rate, which capture must deliver.
  final int sampleRateHz;

  /// Scratch event record.
  final int event;

  /// Scratch packet record.
  final int packet;

  /// Scratch stats record.
  final int stats;

  /// The running capture, if any.
  WebCapture? capture;

  /// Frees the scratch records.
  void free(AurisModule m) {
    m
      ..auris_web_event_free(event)
      ..auris_web_packet_free(packet)
      ..auris_web_stats_free(stats);
  }
}
