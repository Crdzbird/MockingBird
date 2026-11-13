import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/mockingbird_playback_api.dart';
import 'package:mockingbird_web/src/web_player_handle.dart';

/// Volume, mute, flush and stats of a WebAssembly player.
final class WebPlayerControls {
  /// Wraps the loaded module.
  const factory WebPlayerControls(MockingbirdModule m) = WebPlayerControls._;

  const WebPlayerControls._(this._m);

  final MockingbirdModule _m;

  MockingbirdPlaybackApi get _api => MockingbirdPlaybackApi(_m);

  /// Sets the gain.
  int volume(WebPlayerHandle h, double v) => _api.playerSetVolume(h.pointer, v);

  /// Mutes or unmutes.
  int muted(WebPlayerHandle h, {required bool muted}) =>
      _api.playerSetMuted(h.pointer, muted ? 1 : 0);

  /// Starts a new timeline.
  int flush(WebPlayerHandle h) => _api.playerFlush(h.pointer);

  /// Reads every stats field.
  MockingbirdPlayerStatsResult stats(WebPlayerHandle h) {
    int f(int field) => _api.playerStat(h.pointer, field).toInt();
    if (f(0) < 0) {
      return (status: MockingbirdStatusCode.invalidArg, stats: null);
    }
    return (
      status: MockingbirdStatusCode.ok,
      stats: MockingbirdNativePlayerStats(
        packetsReceived: f(0),
        packetsPlayed: f(1),
        packetsConcealed: f(2),
        packetsDropped: f(3),
        packetsLate: f(4),
        underruns: f(5),
        framesRendered: f(6),
        bufferedPackets: f(7),
        bufferedMs: f(8),
        state: f(9),
        outputRestarts: f(10),
      ),
    );
  }
}
