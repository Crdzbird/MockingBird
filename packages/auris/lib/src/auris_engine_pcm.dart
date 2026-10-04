part of 'auris_engine.dart';

/// Raw PCM in and out of an [AurisEngine].
extension AurisEnginePcm on AurisEngine {
  /// Up to [maxSamples] from the PCM tap; needs `AurisQueueConfig.pcmTap`.
  Int16List readPcm({int maxSamples = 4096}) {
    _ensureLive();
    final r = _platform.readPcm(_handle, maxSamples);
    AurisException.check(r.status, 'auris_read_pcm');
    return r.samples;
  }

  /// Feeds audio to an `AurisBackend.hostPcm` engine.
  void pushPcm(Int16List samples, {int captureTsUs = 0}) {
    _ensureLive();
    if (samples.isNotEmpty) {
      AurisException.check(
        _platform.pushPcm(_handle, samples, captureTsUs),
        'auris_push_pcm',
      );
    }
  }
}
