import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/mockingbird_record_api.dart';
import 'package:mockingbird_web/src/web_capture.dart';

/// A WebAssembly engine pointer plus its scratch records and capture.
final class WebEngineHandle extends MockingbirdEngineHandle {
  /// Allocates the scratch records for [pointer], created from [config].
  factory WebEngineHandle(
    MockingbirdModule m,
    int pointer,
    MockingbirdNativeConfig config,
  ) = WebEngineHandle._;

  WebEngineHandle._(
    MockingbirdModule m,
    this.pointer,
    MockingbirdNativeConfig c,
  ) : captures = c.backend != _hostPcm,
      sampleRateHz = c.sampleRateHz,
      voiceProcessing = c.voiceProcessing,
      event = MockingbirdRecordApi(m).eventNew(),
      packet = MockingbirdRecordApi(m).packetNew(),
      stats = MockingbirdRecordApi(m).statsNew();

  static const _hostPcm = 4;

  /// The engine in linear memory.
  final int pointer;

  /// Whether the platform drives the microphone for this engine.
  final bool captures;

  /// The engine's sample rate, which capture must deliver.
  final int sampleRateHz;

  /// The `MOCKINGBIRD_VOICE_*` bits to ask the browser for.
  final int voiceProcessing;

  /// The microphone the next start opens, or null for the default.
  String? deviceId;

  /// Scratch event record.
  final int event;

  /// Scratch packet record.
  final int packet;

  /// Scratch stats record.
  final int stats;

  /// The running capture, if any; it knows what the browser applied.
  WebCapture? capture;

  /// Completes when [capture] has started or failed.
  Future<void>? captureStarted;

  /// Frees the scratch records.
  void free(MockingbirdModule m) {
    MockingbirdRecordApi(m)
      ..eventFree(event)
      ..packetFree(packet)
      ..statsFree(stats);
  }
}
