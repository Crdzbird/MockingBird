/// Which signal statistic tripped the anomaly gate.
enum MockingbirdAnomalyReason {
  /// Loudness jumped against the rolling background.
  rms(1),

  /// The spectrum changed abruptly.
  spectralFlux(2),

  /// The spectral centre of mass shifted.
  centroid(4);

  const MockingbirdAnomalyReason(this.bit);

  /// The bit this reason occupies in a native reason mask.
  final int bit;

  /// Every reason set in a native reason [mask].
  static Set<MockingbirdAnomalyReason> fromMask(int mask) => {
    for (final r in values)
      if (mask & r.bit != 0) r,
  };
}
