import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';
import 'package:mockingbird_platform_interface/src/bindings.dart';
import 'package:mockingbird_platform_interface/src/mockingbird_player_handle.dart';

/// A native player pointer plus the scratch memory pushes and reads reuse,
/// so feeding a player allocates nothing per packet.
final class FfiPlayerHandle extends MockingbirdPlayerHandle {
  /// Wraps [pointer] and allocates the scratch records once.
  factory FfiPlayerHandle(
    ffi.Pointer<mockingbird_player_t> pointer, {
    required int channels,
  }) = FfiPlayerHandle._;

  FfiPlayerHandle._(this.pointer, {required this.channels})
    : bytes = calloc(maxPacketBytes),
      stats = calloc();

  /// The largest packet the C ABI accepts.
  static const int maxPacketBytes = 8192;

  /// The player.
  final ffi.Pointer<mockingbird_player_t> pointer;

  /// Interleaved channels per frame.
  final int channels;

  /// Scratch packet bytes.
  final ffi.Pointer<ffi.Uint8> bytes;

  /// Scratch stats record.
  final ffi.Pointer<mockingbird_player_stats_t> stats;

  ffi.Pointer<ffi.Int16> _pcm = ffi.nullptr;
  int _pcmLength = 0;

  /// A sample buffer of at least [length] values, grown as needed.
  ffi.Pointer<ffi.Int16> pcm(int length) {
    if (length > _pcmLength) {
      if (_pcm != ffi.nullptr) calloc.free(_pcm);
      _pcm = calloc(length);
      _pcmLength = length;
    }
    return _pcm;
  }

  /// Frees the scratch memory; the player itself is destroyed separately.
  void free() {
    calloc
      ..free(bytes)
      ..free(stats);
    if (_pcm != ffi.nullptr) calloc.free(_pcm);
    _pcm = ffi.nullptr;
  }
}
