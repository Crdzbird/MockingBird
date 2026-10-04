/// Which voice-activity detector runs. Both are language-agnostic.
enum AurisVadTier {
  /// libfvad: zero-dependency and cheap.
  fast(0),

  /// Silero v5 via ONNX Runtime: catches faint and borderline speech.
  neural(1);

  const AurisVadTier(this.nativeValue);

  /// The `auris_vad_tier_t` value.
  final int nativeValue;
}
