part of 'mockingbird_engine.dart';

/// Live retuning of a running [MockingbirdEngine].
extension MockingbirdEngineTuning on MockingbirdEngine {
  /// Applies the tunable subset of [config] in place: detector toggles and
  /// thresholds, the silence gate, degradation and wake batching. Detector
  /// state survives, so an utterance in progress is not cut. Structural
  /// fields must match the creating config; a detector that was off at
  /// creation cannot be switched on.
  void updateConfig(MockingbirdConfig config) {
    _ensureLive();
    MockingbirdException.check(
      _platform.updateConfig(_handle, const NativeConfigMapper().map(config)),
      'mockingbird_update_config',
    );
    _config = config;
  }
}
