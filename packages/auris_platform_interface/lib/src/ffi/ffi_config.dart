import 'dart:ffi' as ffi;

import 'package:auris_platform_interface/src/auris_native_config.dart';
import 'package:auris_platform_interface/src/bindings.dart';
import 'package:ffi/ffi.dart';

/// An `auris_config_t` in native memory filled from an [AurisNativeConfig].
final class FfiConfig {
  FfiConfig._(this.pointer, this._neuralPath, this._soundPath);

  /// Allocates the struct, applies the engine defaults, then every field.
  factory FfiConfig.from(AurisBindings bindings, AurisNativeConfig c) {
    final d = c.detectors;
    final o = c.output;
    final p = calloc<auris_config_t>();
    bindings.auris_config_defaults(p);
    final neural = _utf8(d.neuralVadModelPath);
    final sound = _utf8(d.soundClassModelPath);
    p.ref
      ..backendAsInt = c.backend
      ..sample_rate_hz = c.sampleRateHz
      ..frame_ms = c.frameMs
      ..enable_rnnoise = _flag(c.enableRnnoise)
      ..enable_vad = _flag(d.enableVad)
      ..vad_tierAsInt = d.vadTier
      ..vad_aggressiveness = d.vadAggressiveness
      ..vad_neural_threshold = d.vadNeuralThreshold
      ..vad_hangover_ms = d.vadHangoverMs
      ..neural_vad_model_path = neural?.cast() ?? ffi.nullptr
      ..enable_anomaly = _flag(d.enableAnomaly)
      ..anomaly_rms_z = d.anomalyRmsZ
      ..anomaly_flux_z = d.anomalyFluxZ
      ..anomaly_background_ms = d.anomalyBackgroundMs
      ..enable_sound_class = _flag(d.enableSoundClass)
      ..sound_class_model_path = sound?.cast() ?? ffi.nullptr
      ..sound_class_threshold = d.soundClassThreshold
      ..sound_class_interval_ms = d.soundClassIntervalMs
      ..pre_roll_ms = o.preRollMs
      ..post_roll_ms = o.postRollMs
      ..segment_max_ms = o.segmentMaxMs
      ..codecAsInt = o.codec
      ..opus_bitrate_bps = o.opusBitrateBps
      ..opus_vbr = _flag(o.opusVbr)
      ..opus_complexity = o.opusComplexity
      ..pcm_ring_frames = o.pcmRingFrames
      ..packet_queue_capacity = o.packetQueueCapacity
      ..enable_pcm_tap = _flag(o.enablePcmTap)
      ..pcm_tap_frames = o.pcmTapFrames
      ..silence_gate_dbfs = o.silenceGateDbfs
      ..enable_adaptive_degradation = _flag(o.enableAdaptiveDegradation)
      ..degrade_busy_ppm = o.degradeBusyPpm
      ..worker_batch_frames = o.workerBatchFrames;
    return FfiConfig._(p, neural, sound);
  }

  /// The filled struct.
  final ffi.Pointer<auris_config_t> pointer;
  final ffi.Pointer<Utf8>? _neuralPath;
  final ffi.Pointer<Utf8>? _soundPath;

  /// Releases the struct and the strings it points to.
  void free() {
    calloc.free(pointer);
    if (_neuralPath case final p?) calloc.free(p);
    if (_soundPath case final p?) calloc.free(p);
  }

  static int _flag(bool value) => value ? 1 : 0;

  static ffi.Pointer<Utf8>? _utf8(String? s) =>
      s == null || s.isEmpty ? null : s.toNativeUtf8();
}
