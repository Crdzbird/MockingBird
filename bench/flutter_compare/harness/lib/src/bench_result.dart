import 'dart:convert';

class BenchResult {
  const BenchResult({
    required this.library,
    required this.scenario,
    required this.audioSeconds,
    required this.cpuSeconds,
    required this.wallSeconds,
    required this.rssBaseMb,
    required this.rssPeakMb,
    required this.counters,
  });

  final String library;
  final String scenario;
  final double audioSeconds;
  final double cpuSeconds;
  final double wallSeconds;
  final double rssBaseMb;
  final double rssPeakMb;
  final Map<String, num> counters;

  double get cpuShare => cpuSeconds / audioSeconds;

  Map<String, Object> toJson() => {
        'library': library,
        'scenario': scenario,
        'audio_s': audioSeconds,
        'cpu_s': cpuSeconds,
        'wall_s': wallSeconds,
        'cpu_share': cpuShare,
        'rss_base_mb': rssBaseMb,
        'rss_peak_mb': rssPeakMb,
        'counters': counters,
      };

  String toJsonLine() => jsonEncode(toJson());
}
