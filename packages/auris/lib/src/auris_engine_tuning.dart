part of 'auris_engine.dart';

/// Live retuning of a running [AurisEngine].
extension AurisEngineTuning on AurisEngine {
  /// Applies the tunable subset of [config] in place: detector toggles and
  /// thresholds, the silence gate, degradation and wake batching. Detector
  /// state survives, so an utterance in progress is not cut. Structural
  /// fields must match the creating config; a detector that was off at
  /// creation cannot be switched on.
  void updateConfig(AurisConfig config) {
    _ensureLive();
    final native = NativeConfig.from(_bindings, config);
    try {
      AurisException.check(
        _bindings.auris_update_config(_engine, native.pointer),
        'auris_update_config',
      );
      _config = config;
    } finally {
      native.free();
    }
  }
}
