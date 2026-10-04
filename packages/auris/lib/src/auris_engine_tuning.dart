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
    AurisException.check(
      _platform.updateConfig(_handle, const NativeConfigMapper().map(config)),
      'auris_update_config',
    );
    _config = config;
  }
}
