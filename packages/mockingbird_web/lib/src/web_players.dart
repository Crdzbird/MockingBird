import 'dart:js_interop';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/mockingbird_playback_api.dart';
import 'package:mockingbird_web/src/web_audio_output.dart';
import 'package:mockingbird_web/src/web_player_feed.dart';
import 'package:mockingbird_web/src/web_player_handle.dart';

/// Player operations over the WebAssembly module, with Web Audio pulling
/// the output of every player that is not host PCM.
final class WebPlayers {
  /// Wraps the loaded module.
  const factory WebPlayers(MockingbirdModule m) = WebPlayers._;

  const WebPlayers._(this._m);

  final MockingbirdModule _m;

  MockingbirdPlaybackApi get _api => MockingbirdPlaybackApi(_m);

  /// Creates a player from [c].
  MockingbirdPlayerCreateResult create(MockingbirdNativePlayerConfig c) {
    final statusPtr = _m.malloc(4);
    try {
      final p = _api.playerCreate(
        c.sampleRateHz,
        c.channels,
        c.targetDelayMs,
        c.maxDelayMs,
        c.concealMaxMs,
        c.queuePackets,
        statusPtr,
      );
      final status = _m.heap32.toDart[statusPtr >> 2];
      if (p == 0) return (status: status, handle: null);
      return (status: status, handle: WebPlayerHandle(_m, p, c));
    } finally {
      _m.free(statusPtr);
    }
  }

  /// Closes the output, destroys the player and frees its scratch memory.
  Future<void> destroy(WebPlayerHandle h) async {
    await h.output?.close();
    h.output = null;
    _api.playerDestroy(h.pointer);
    h.free(_m);
  }

  /// Starts playout and, unless host PCM, the Web Audio output.
  Future<int> start(WebPlayerHandle h) async {
    final s = _api.playerStart(h.pointer);
    if (s != MockingbirdStatusCode.ok || !h.plays) return s;
    final output = h.output ??= WebAudioOutput(
      sampleRateHz: h.sampleRateHz,
      channels: h.channels,
      pull: (frames) => WebPlayerFeed(_m).read(h, frames).samples,
    );
    await output.start();
    return s;
  }

  /// Pauses playout and suspends the output.
  Future<int> pause(WebPlayerHandle h) async {
    final s = _api.playerPause(h.pointer);
    await h.output?.pause();
    return s;
  }

  /// Resumes playout and the output.
  Future<int> resume(WebPlayerHandle h) async {
    final s = _api.playerResume(h.pointer);
    if (s == MockingbirdStatusCode.ok) await h.output?.start();
    return s;
  }

  /// Closes the output and stops the player.
  Future<int> stop(WebPlayerHandle h) async {
    await h.output?.close();
    h.output = null;
    return _api.playerStop(h.pointer);
  }
}
