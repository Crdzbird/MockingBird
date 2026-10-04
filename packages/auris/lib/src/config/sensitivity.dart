/// One 0..1 dial that drives every detector's thresholds at once.
///
/// 0 is the least sensitive (fewest detections, fewest false positives);
/// 1 catches faint and borderline sounds at the cost of more false positives.
extension type const Sensitivity._(double value) {
  /// Clamps [raw] into 0..1.
  Sensitivity(double raw) : this._(raw.clamp(0, 1).toDouble());

  /// The value a threshold takes at this sensitivity, moving from [atZero]
  /// (least sensitive) to [atOne] (most sensitive).
  double lerp(double atZero, double atOne) => atZero + (atOne - atZero) * value;
}
