part of 'auris_engine.dart';

/// Raw PCM in and out of an [AurisEngine].
extension AurisEnginePcm on AurisEngine {
  /// Up to [maxSamples] from the PCM tap; needs `AurisQueueConfig.pcmTap`.
  Int16List readPcm({int maxSamples = 4096}) {
    _ensureLive();
    return NativePcm(_bindings, _engine).read(maxSamples);
  }

  /// Feeds audio to an `AurisBackend.hostPcm` engine.
  void pushPcm(Int16List samples, {int captureTsUs = 0}) {
    _ensureLive();
    if (samples.isNotEmpty) {
      NativePcm(_bindings, _engine).push(samples, captureTsUs);
    }
  }
}
