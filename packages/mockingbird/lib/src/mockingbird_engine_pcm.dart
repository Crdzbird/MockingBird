part of 'mockingbird_engine.dart';

/// Raw PCM in and out of a [MockingbirdEngine].
extension MockingbirdEnginePcm on MockingbirdEngine {
  /// Up to [maxSamples] from the PCM tap; needs
  /// `MockingbirdQueueConfig.pcmTap`.
  Int16List readPcm({int maxSamples = 4096}) {
    _ensureLive();
    final r = _platform.readPcm(_handle, maxSamples);
    MockingbirdException.check(r.status, 'mockingbird_read_pcm');
    return r.samples;
  }

  /// Feeds audio to a `MockingbirdBackend.hostPcm` engine.
  void pushPcm(Int16List samples, {int captureTsUs = 0}) {
    _ensureLive();
    if (samples.isNotEmpty) {
      MockingbirdException.check(
        _platform.pushPcm(_handle, samples, captureTsUs),
        'mockingbird_push_pcm',
      );
    }
  }
}
