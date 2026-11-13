import 'package:equatable/equatable.dart';
import 'package:mockingbird/mockingbird.dart';

/// What the playback card renders.
final class PlaybackState extends Equatable {
  const factory PlaybackState({
    bool playing,
    Duration kept,
    MockingbirdPlayerStats? stats,
    String? error,
  }) = PlaybackState._;

  const PlaybackState._({
    this.playing = false,
    this.kept = Duration.zero,
    this.stats,
    this.error,
  });

  final bool playing;
  final Duration kept;
  final MockingbirdPlayerStats? stats;
  final String? error;

  PlaybackState copyWith({
    bool? playing,
    Duration? kept,
    MockingbirdPlayerStats? stats,
    String? error,
  }) => PlaybackState(
    playing: playing ?? this.playing,
    kept: kept ?? this.kept,
    stats: stats ?? this.stats,
    error: error,
  );

  @override
  List<Object?> get props => [playing, kept, stats, error];
}
