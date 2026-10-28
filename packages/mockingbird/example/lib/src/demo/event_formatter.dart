import 'package:mockingbird/mockingbird.dart';

/// Turns events into log lines, naming YAMNet classes from the label map.
final class EventFormatter {
  const factory EventFormatter(List<String> labels) = EventFormatter._;

  const EventFormatter._(this.labels);

  final List<String> labels;

  /// The AudioSet label for [classId], or a placeholder.
  String labelFor(int classId) => classId >= 0 && classId < labels.length
      ? labels[classId]
      : 'class $classId';

  /// One console-style line for [event].
  String format(MockingbirdEvent event) => switch (event) {
    MockingbirdSoundClassEvent(
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
