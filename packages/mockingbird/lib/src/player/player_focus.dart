import 'dart:async';

import 'package:mockingbird_platform_interface/mockingbird_platform_interface.dart';

/// Turns audio focus changes into pause, resume and ducking. A transient
/// loss resumes when focus comes back unless the app paused meanwhile; a
/// permanent loss and a lost route (`becomingNoisy`) wait for the app.
final class PlayerFocus {
  /// Calls [onPause], [onResume] and [onDuck] as focus changes.
  factory PlayerFocus({
    required Future<void> Function() onPause,
    required Future<void> Function() onResume,
    required void Function() onDuck,
  }) = PlayerFocus._;

  PlayerFocus._({
    required this.onPause,
    required this.onResume,
    required this.onDuck,
  });

  /// Pauses the output.
  final Future<void> Function() onPause;

  /// Resumes the output and restores the volume.
  final Future<void> Function() onResume;

  /// Lowers the volume.
  final void Function() onDuck;

  StreamSubscription<void>? _sub;
  bool _pausedByUser = false;

  /// Starts following [changes].
  void follow(Stream<MockingbirdPlaybackFocus> changes) {
    _pausedByUser = false;
    _sub = changes.asyncMap(_on).listen(null);
  }

  /// The app paused; focus coming back must not resume it.
  void userPaused() => _pausedByUser = true;

  /// The app resumed.
  void userResumed() => _pausedByUser = false;

  /// Stops following.
  Future<void> cancel() async {
    await _sub?.cancel();
    _sub = null;
  }

  Future<void> _on(MockingbirdPlaybackFocus f) async {
    switch (f) {
      case MockingbirdPlaybackFocus.lostTransient:
        await onPause();
      case MockingbirdPlaybackFocus.lost ||
          MockingbirdPlaybackFocus.becomingNoisy:
        _pausedByUser = true;
        await onPause();
      case MockingbirdPlaybackFocus.duck:
        onDuck();
      case MockingbirdPlaybackFocus.gained:
        if (!_pausedByUser) await onResume();
    }
  }
}
