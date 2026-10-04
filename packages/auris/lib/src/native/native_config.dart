import 'dart:ffi' as ffi;

import 'package:auris/src/auris_exception.dart';
import 'package:auris/src/config/auris_config.dart';
import 'package:auris_platform_interface/auris_platform_interface.dart';
import 'package:ffi/ffi.dart';

/// A native `auris_config_t` and the UTF-8 buffers it borrows. Call [free]
/// as soon as the native call returns: the engine copies what it keeps.
final class NativeConfig {
  NativeConfig._(this.pointer, this._neuralPath, this._soundPath);

  /// Marshals [config] with [bindings].
  factory NativeConfig.from(AurisBindings bindings, AurisConfig config) {
    final p = calloc<auris_config_t>();
    AurisException.check(
      bindings.auris_config_defaults(p),
      'auris_config_defaults',
    );
    final neural = _utf8(config.vad.modelPath);
    final sound = _utf8(config.soundClass.modelPath);
    p.ref
      ..backendAsInt = config.backend.nativeValue
      ..sample_rate_hz = config.sampleRateHz
      ..frame_ms = config.frameMs
      ..enable_rnnoise = _flag(config.denoise)
      ..enable_vad = _flag(config.vad.enabled)
      ..vad_tierAsInt = config.vad.tier.nativeValue
      ..vad_aggressiveness = config.vad.aggressiveness
      ..vad_neural_threshold = config.vad.neuralThreshold
      ..vad_hangover_ms = config.vad.hangoverMs
      ..neural_vad_model_path = neural?.cast() ?? ffi.nullptr
      ..enable_anomaly = _flag(config.anomaly.enabled)
      ..anomaly_rms_z = config.anomaly.rmsZ
      ..anomaly_flux_z = config.anomaly.fluxZ
      ..anomaly_background_ms = config.anomaly.backgroundMs
      ..enable_sound_class = _flag(config.soundClass.enabled)
      ..sound_class_model_path = sound?.cast() ?? ffi.nullptr
      ..sound_class_threshold = config.soundClass.threshold
      ..sound_class_interval_ms = config.soundClass.intervalMs
      ..pre_roll_ms = config.segments.preRollMs
      ..post_roll_ms = config.segments.postRollMs
      ..segment_max_ms = config.segments.maxMs
      ..codecAsInt = config.encoder.codec.nativeValue
      ..opus_bitrate_bps = config.encoder.opusBitrateBps
      ..opus_vbr = _flag(config.encoder.opusVbr)
      ..opus_complexity = config.encoder.opusComplexity
      ..pcm_ring_frames = config.queues.pcmRingFrames
      ..packet_queue_capacity = config.queues.packetQueueCapacity
      ..enable_pcm_tap = _flag(config.queues.pcmTap)
      ..pcm_tap_frames = config.queues.pcmTapFrames
      ..silence_gate_dbfs = config.performance.silenceGateDbfs
      ..enable_adaptive_degradation = _flag(
        config.performance.adaptiveDegradation,
      )
      ..degrade_busy_ppm = config.performance.degradeBusyPpm
      ..worker_batch_frames = config.performance.workerBatchFrames;
    return NativeConfig._(p, neural, sound);
  }

  /// The struct to pass to the engine.
  final ffi.Pointer<auris_config_t> pointer;

  final ffi.Pointer<Utf8>? _neuralPath;
  final ffi.Pointer<Utf8>? _soundPath;

  /// Releases the struct and its string buffers.
  void free() {
    calloc.free(pointer);
    if (_neuralPath case final p?) calloc.free(p);
    if (_soundPath case final p?) calloc.free(p);
  }

  static int _flag(bool value) => value ? 1 : 0;

  static ffi.Pointer<Utf8>? _utf8(String? s) =>
      s == null || s.isEmpty ? null : s.toNativeUtf8();
}
