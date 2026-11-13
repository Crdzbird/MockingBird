import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_example/src/demo/event_formatter.dart';

/// The model paths and class labels an engine session needs.
final class DemoModels {
  const factory DemoModels({
    EventFormatter formatter,
    String neuralVadModelPath,
    String soundClassModelPath,
  }) = DemoModels._;

  const DemoModels._({
    this.formatter = const EventFormatter([]),
    this.neuralVadModelPath = '',
    this.soundClassModelPath = '',
  });

  /// Resolves the bundled and downloaded models.
  static Future<DemoModels> load() async => DemoModels(
    formatter: EventFormatter(await MockingbirdModels.yamnetLabels()),
    neuralVadModelPath: await MockingbirdModels.neuralVadPath(),
    soundClassModelPath: await MockingbirdModels.soundClassPath(),
  );

  final EventFormatter formatter;
  final String neuralVadModelPath;
  final String soundClassModelPath;
}
