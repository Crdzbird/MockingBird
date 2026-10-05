import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/auris_module.dart';
import 'package:mockingbird_web/src/auris_record_api.dart';
import 'package:mockingbird_web/src/web_capture.dart';

/// A WebAssembly engine pointer plus its scratch records and capture.
final class WebEngineHandle extends AurisEngineHandle {
  /// Allocates the scratch records for [pointer].
  WebEngineHandle(
    AurisModule m,
    this.pointer, {
    required this.captures,
    required this.sampleRateHz,
  }) : event = AurisRecordApi(m).eventNew(),
       packet = AurisRecordApi(m).packetNew(),
       stats = AurisRecordApi(m).statsNew();

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

  /// Completes when [capture] has started or failed.
  Future<void>? captureStarted;

  /// Frees the scratch records.
  void free(AurisModule m) {
    AurisRecordApi(m)
      ..eventFree(event)
      ..packetFree(packet)
      ..statsFree(stats);
  }
}
