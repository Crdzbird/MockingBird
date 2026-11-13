/// A `mockingbird_player_stats_t` as plain integers.
final class MockingbirdNativePlayerStats {
  /// Creates a snapshot.
  const factory MockingbirdNativePlayerStats({
    required int packetsReceived,
    required int packetsPlayed,
    required int packetsConcealed,
    required int packetsDropped,
    required int packetsLate,
    required int underruns,
    required int framesRendered,
    required int bufferedPackets,
    required int bufferedMs,
    required int state,
    required int outputRestarts,
  }) = MockingbirdNativePlayerStats._;

  const MockingbirdNativePlayerStats._({
    required this.packetsReceived,
    required this.packetsPlayed,
    required this.packetsConcealed,
    required this.packetsDropped,
    required this.packetsLate,
    required this.underruns,
    required this.framesRendered,
    required this.bufferedPackets,
    required this.bufferedMs,
    required this.state,
    required this.outputRestarts,
  });

  /// Packets push accepted.
  final int packetsReceived;

  /// Packets decoded and played.
  final int packetsPlayed;

  /// Missing packets replaced by concealment.
  final int packetsConcealed;

  /// Late, duplicate, over the delay cap, without a slot, or undecodable.
  final int packetsDropped;

  /// The late and duplicate share of [packetsDropped].
  final int packetsLate;

  /// Times the output ran dry while playing.
  final int underruns;

  /// Frames given to the output, silence included.
  final int framesRendered;

  /// Packets waiting in the jitter buffer.
  final int bufferedPackets;

  /// Audio waiting in the jitter buffer, milliseconds.
  final int bufferedMs;

  /// `mockingbird_player_state_t`.
  final int state;

  /// Route changes the output followed.
  final int outputRestarts;
}
