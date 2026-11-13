import 'package:mockingbird/src/player/mockingbird_player_state.dart';
import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// A snapshot of a player's counters.
final class MockingbirdPlayerStats {
  /// Creates a snapshot.
  const factory MockingbirdPlayerStats({
    required int received,
    required int played,
    required int concealed,
    required int dropped,
    required int late,
    required int underruns,
    required int framesRendered,
    required int bufferedPackets,
    required Duration buffered,
    required MockingbirdPlayerState state,
    required int outputRestarts,
  }) = MockingbirdPlayerStats._;

  const MockingbirdPlayerStats._({
    required this.received,
    required this.played,
    required this.concealed,
    required this.dropped,
    required this.late,
    required this.underruns,
    required this.framesRendered,
    required this.bufferedPackets,
    required this.buffered,
    required this.state,
    required this.outputRestarts,
  });

  /// Copies a native snapshot.
  factory MockingbirdPlayerStats.fromNative(MockingbirdNativePlayerStats s) =>
      MockingbirdPlayerStats(
        received: s.packetsReceived,
        played: s.packetsPlayed,
        concealed: s.packetsConcealed,
        dropped: s.packetsDropped,
        late: s.packetsLate,
        underruns: s.underruns,
        framesRendered: s.framesRendered,
        bufferedPackets: s.bufferedPackets,
        buffered: Duration(milliseconds: s.bufferedMs),
        state: MockingbirdPlayerState.fromNative(s.state),
        outputRestarts: s.outputRestarts,
      );

  /// Packets the player accepted.
  final int received;

  /// Packets decoded and played.
  final int played;

  /// Missing packets replaced by concealment.
  final int concealed;

  /// Late, duplicate, over the delay cap, without a slot, or undecodable.
  final int dropped;

  /// The late and duplicate share of [dropped].
  final int late;

  /// Times the output ran dry while playing.
  final int underruns;

  /// Frames given to the output, silence included.
  final int framesRendered;

  /// Packets waiting to play.
  final int bufferedPackets;

  /// Audio waiting to play.
  final Duration buffered;

  /// What the player is doing.
  final MockingbirdPlayerState state;

  /// Route changes the output followed.
  final int outputRestarts;
}
