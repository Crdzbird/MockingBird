import 'package:flutter/services.dart' show rootBundle;

/// YAMNet's class names, read once from the bundled class map.
final class YamnetLabels {
  /// Creates the reader.
  YamnetLabels();

  List<String>? _labels;

  /// The 521 labels, indexed by class id, from asset [key].
  Future<List<String>> load(String key) async =>
      _labels ??= _parse(await rootBundle.loadString(key));

  static List<String> _parse(String text) => [
    for (final line in text.split('\n'))
      if (line.trim() case final label when label.isNotEmpty)
        label.substring(label.indexOf(' ') + 1),
  ];
}
