import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/auris_module.dart';
import 'package:mockingbird_web/src/auris_record_api.dart';
import 'package:mockingbird_web/src/web_config_field.dart';

/// Writes an [AurisNativeConfig] into a native config through the shim.
///
/// Capture is always host-pushed in the WebAssembly build; the platform
/// drives the microphone itself when the caller asked for a capture backend.
final class WebConfigWriter {
  /// Wraps the loaded module.
  const WebConfigWriter(this._m);

  static const _hostPcm = 4;

  final AurisModule _m;

  /// Allocates and fills a config; the caller frees it.
  int write(AurisNativeConfig c) {
    final d = c.detectors;
    final o = c.output;
    final cfg = AurisRecordApi(_m).configNew();
    final values = <WebConfigField, num>{
      WebConfigField.backend: _hostPcm,
      WebConfigField.sampleRateHz: c.sampleRateHz,
      WebConfigField.frameMs: c.frameMs,
      WebConfigField.enableRnnoise: _flag(c.enableRnnoise),
      WebConfigField.enableVad: _flag(d.enableVad),
      WebConfigField.vadTier: d.vadTier,
      WebConfigField.vadAggressiveness: d.vadAggressiveness,
      WebConfigField.vadNeuralThreshold: d.vadNeuralThreshold,
      WebConfigField.vadHangoverMs: d.vadHangoverMs,
      WebConfigField.enableAnomaly: _flag(d.enableAnomaly),
      WebConfigField.anomalyRmsZ: d.anomalyRmsZ,
      WebConfigField.anomalyFluxZ: d.anomalyFluxZ,
      WebConfigField.anomalyBackgroundMs: d.anomalyBackgroundMs,
      WebConfigField.enableSoundClass: _flag(d.enableSoundClass),
      WebConfigField.soundClassThreshold: d.soundClassThreshold,
      WebConfigField.soundClassIntervalMs: d.soundClassIntervalMs,
      WebConfigField.preRollMs: o.preRollMs,
      WebConfigField.postRollMs: o.postRollMs,
      WebConfigField.segmentMaxMs: o.segmentMaxMs,
      WebConfigField.codec: o.codec,
      WebConfigField.opusBitrateBps: o.opusBitrateBps,
      WebConfigField.opusVbr: _flag(o.opusVbr),
      WebConfigField.opusComplexity: o.opusComplexity,
      WebConfigField.pcmRingFrames: o.pcmRingFrames,
      WebConfigField.packetQueueCapacity: o.packetQueueCapacity,
      WebConfigField.enablePcmTap: _flag(o.enablePcmTap),
      WebConfigField.pcmTapFrames: o.pcmTapFrames,
      WebConfigField.silenceGateDbfs: o.silenceGateDbfs,
      WebConfigField.enableAdaptiveDegradation: _flag(
        o.enableAdaptiveDegradation,
      ),
      WebConfigField.degradeBusyPpm: o.degradeBusyPpm,
      WebConfigField.workerBatchFrames: o.workerBatchFrames,
    };
    for (final MapEntry(:key, :value) in values.entries) {
      AurisRecordApi(_m).configSet(cfg, key.index, value.toDouble());
    }
    return cfg;
  }

  static int _flag(bool v) => v ? 1 : 0;
}
