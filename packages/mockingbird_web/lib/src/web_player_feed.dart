import 'dart:js_interop';
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';
import 'package:mockingbird_web/src/mockingbird_module.dart';
import 'package:mockingbird_web/src/mockingbird_playback_api.dart';
import 'package:mockingbird_web/src/web_player_handle.dart';

/// Packets into a WebAssembly player and samples out of it, through the
/// handle's scratch memory.
final class WebPlayerFeed {
  /// Wraps the loaded module.
  const factory WebPlayerFeed(MockingbirdModule m) = WebPlayerFeed._;

  const WebPlayerFeed._(this._m);

  final MockingbirdModule _m;

  MockingbirdPlaybackApi get _api => MockingbirdPlaybackApi(_m);

  /// Copies [data] into linear memory and queues it.
  int push(WebPlayerHandle h, int codec, Uint8List data, int ptsUs, int flags) {
    if (data.isEmpty || data.length > WebPlayerHandle.maxPacketBytes) {
      return MockingbirdStatusCode.invalidArg;
    }
    _m.heapU8.toDart.setAll(h.bytes, data);
    return _api.playerPush(
      h.pointer,
      codec,
      h.bytes,
      data.length,
      ptsUs.toDouble(),
      flags,
    );
  }

  /// Renders [frames] frames into a Dart-owned list.
  MockingbirdPcmResult read(WebPlayerHandle h, int frames) {
    if (frames < 0 || frames > h.sampleRateHz) {
      return (status: MockingbirdStatusCode.invalidArg, samples: Int16List(0));
    }
    final s = _api.playerRead(h.pointer, h.pcm, frames);
    if (s != MockingbirdStatusCode.ok) {
      return (status: s, samples: Int16List(0));
    }
    final start = h.pcm >> 1;
    final end = start + frames * h.channels;
    return (status: s, samples: _m.heap16.toDart.sublist(start, end));
  }
}
