import 'package:equatable/equatable.dart';
import 'package:mockingbird/mockingbird.dart';
import 'package:mockingbird_example/src/demo/event_formatter.dart';

/// What the detectors currently say: voice state from Silero VAD, the latest
/// recognised sound from YAMNet.
final class DetectionSummary extends Equatable {
  const factory DetectionSummary({
    bool voiceActive,
    String lastSound,
    MockingbirdSoundCategory lastCategory,
    double lastConfidence,
  }) = DetectionSummary._;

  const DetectionSummary._({
    this.voiceActive = false,
    this.lastSound = '-',
    this.lastCategory = .other,
    this.lastConfidence = 0,
  });

  final bool voiceActive;
  final String lastSound;
  final MockingbirdSoundCategory lastCategory;
  final double lastConfidence;

  /// The summary after [event]. A human-voice classification also lights
  /// the voice lamp, so voice shows even before Silero fires.
  DetectionSummary applyEvent(
    MockingbirdEvent event,
    EventFormatter formatter,
  ) => switch (event) {
    MockingbirdSpeechBeginEvent() => copyWith(voiceActive: true),
    MockingbirdSpeechEndEvent() => copyWith(voiceActive: false),
    MockingbirdSoundClassEvent(
      :final classId,
      :final category,
      :final confidence,
      :final isHumanVoice,
    ) =>
      DetectionSummary(
        voiceActive: voiceActive || isHumanVoice,
        lastSound: formatter.labelFor(classId),
        lastCategory: category,
        lastConfidence: confidence,
      ),
    _ => this,
  };

  DetectionSummary copyWith({bool? voiceActive}) => DetectionSummary(
    voiceActive: voiceActive ?? this.voiceActive,
    lastSound: lastSound,
    lastCategory: lastCategory,
    lastConfidence: lastConfidence,
  );

  @override
  List<Object?> get props => [
    voiceActive,
    lastSound,
    lastCategory,
    lastConfidence,
  ];
}
