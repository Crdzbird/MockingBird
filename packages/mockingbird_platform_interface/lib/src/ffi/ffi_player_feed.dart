import 'dart:ffi' as ffi;
import 'dart:typed_data';

import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/ffi/ffi_player_handle.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_results.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_status_code.dart';

/// Packets into a player and, for host PCM, samples out of it, through the
/// handle's scratch memory.
final class FfiPlayerFeed {
  /// Wraps [_bindings].
  const factory FfiPlayerFeed(MockingbirdBindings bindings) = FfiPlayerFeed._;

  const FfiPlayerFeed._(this._bindings);

  final MockingbirdBindings _bindings;

  /// Copies [data] into native memory and queues it.
  int push(
    FfiPlayerHandle h, {
    required int codec,
    required Uint8List data,
    required int ptsUs,
    required int flags,
  }) {
    final bad = codec < 0 || codec > 3;
    if (bad || data.isEmpty || data.length > FfiPlayerHandle.maxPacketBytes) {
      return MockingbirdStatusCode.invalidArg;
    }
    h.bytes.asTypedList(data.length).setAll(0, data);
    return _bindings
        .mockingbird_player_push(
          h.pointer,
          mockingbird_codec.fromValue(codec),
          h.bytes,
          data.length,
          ptsUs,
          flags,
        )
        .value;
  }

  /// Renders [frames] frames into a Dart-owned list.
  MockingbirdPcmResult read(FfiPlayerHandle h, int frames) {
    final channels = h.channels;
    final buf = h.pcm(frames * channels);
    final s = _bindings.mockingbird_player_read(h.pointer, buf, frames).value;
    if (s != MockingbirdStatusCode.ok) {
      return (status: s, samples: Int16List(0));
    }
    return (
      status: s,
      samples: Int16List.fromList(buf.asTypedList(frames * channels)),
    );
  }
}
