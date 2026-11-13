/// What the operating system says about the app's right to play audio, on
/// the platforms that have such a notion (Android audio focus).
enum MockingbirdPlaybackFocus {
  /// Focus is back after a loss; playback paused for it may resume.
  gained,

  /// Another app took focus for good; playback should pause.
  lost,

  /// Focus is gone for a while, a call or a navigation prompt; pause.
  lostTransient,

  /// Another app plays briefly over this one; play quieter.
  duck,

  /// The headphones or Bluetooth output went away; pause.
  becomingNoisy,
}
