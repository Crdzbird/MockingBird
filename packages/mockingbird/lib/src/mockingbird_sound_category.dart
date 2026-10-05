/// A broad category for one of YAMNet's 521 AudioSet classes.
///
/// AudioSet is English-biased: non-English speech often lands on
/// "Conversation" or "Narration" rather than "Speech". All of those map to
/// [speech], which keeps "is this a voice?" decisions language-agnostic.
enum MockingbirdSoundCategory {
  /// Human speech in any language (classes 0..12).
  speech,

  /// Laughter, crying, singing, breathing, footsteps (13..66).
  humanOther,

  /// Animal sounds (67..131).
  animal,

  /// Music and instruments (132..282).
  music,

  /// Wind, water, thunder, fire (283..299).
  nature,

  /// Vehicles, alarms, tools, glass, and silence (494).
  other;

  /// The category of a YAMNet AudioSet class index.
  static MockingbirdSoundCategory of(int classId) => switch (classId) {
    >= 0 && <= 12 => speech,
    >= 13 && <= 66 => humanOther,
    >= 67 && <= 131 => animal,
    >= 132 && <= 282 => music,
    >= 283 && <= 299 => nature,
    _ => other,
  };

  /// True for any human vocalisation in any language: speech, laughter,
  /// crying, singing and humming (classes 0..34).
  static bool isHumanVoice(int classId) => classId >= 0 && classId <= 34;
}
