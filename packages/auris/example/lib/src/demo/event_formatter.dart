import 'package:auris/auris.dart';

/// Turns events into log lines, naming YAMNet classes from the label map.
final class EventFormatter {
  const EventFormatter(this.labels);

  final List<String> labels;

  /// The AudioSet label for [classId], or a placeholder.
  String labelFor(int classId) => classId >= 0 && classId < labels.length
      ? labels[classId]
      : 'class $classId';

  /// One console-style line for [event].
  String format(AurisEvent event) => switch (event) {
    AurisSoundClassEvent(
      :final timestampUs,
      :final classId,
      :final category,
      :final confidence,
    ) =>
      '[$timestampUs us] SOUND_CLASS [${category.name.toUpperCase()}] '
          '"${labelFor(classId)}" conf=${confidence.toStringAsFixed(2)}',
    _ => '$event',
  };
}
