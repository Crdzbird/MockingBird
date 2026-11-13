part of 'mockingbird_player.dart';

/// Packets into a [MockingbirdPlayer], and PCM out of a host-PCM one.
extension MockingbirdPlayerFeed on MockingbirdPlayer {
  /// Queues an engine packet at its capture time, which is one clock for
  /// the whole run, unlike its per-segment pts.
  void push(MockingbirdPacket packet) => _push(
    packet.codec.nativeValue,
    packet.data,
    packet.captureTsUs,
    packet.flags,
  );

  /// Queues raw Opus bytes at [ptsUs]; [last] ends the talkspurt.
  void pushOpus(Uint8List opus, {required int ptsUs, bool last = false}) =>
      _push(1, opus, ptsUs, last ? _lastFlag : 0);

  /// Queues interleaved samples at the player's rate at [ptsUs].
  void pushPcm(Int16List samples, {required int ptsUs, bool last = false}) =>
      _push(
        0,
        samples.buffer.asUint8List(
          samples.offsetInBytes,
          samples.lengthInBytes,
        ),
        ptsUs,
        last ? _lastFlag : 0,
      );

  /// Drops everything pushed so far and starts a new timeline.
  void flush() {
    _ensureLive();
    MockingbirdException.check(
      _platform.playerFlush(_handle),
      'mockingbird_player_flush',
    );
  }

  /// Renders [frames] frames from a `MockingbirdBackend.hostPcm` player;
  /// silence where there is nothing to play.
  Int16List read(int frames) {
    _ensureLive();
    final r = _platform.playerRead(_handle, frames);
    MockingbirdException.check(r.status, 'mockingbird_player_read');
    return r.samples;
  }

  static const int _lastFlag = 8;

  void _push(int codec, Uint8List data, int ptsUs, int flags) {
    _ensureLive();
    MockingbirdException.check(
      _platform.playerPush(
        _handle,
        codec: codec,
        data: data,
        ptsUs: ptsUs,
        flags: flags,
      ),
      'mockingbird_player_push',
    );
  }
}
