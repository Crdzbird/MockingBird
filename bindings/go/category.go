package auris

// SoundCategory is a broad grouping of YAMNet's 521 fine-grained AudioSet
// classes. YAMNet splits a single perceptual category across many labels; in
// particular the human-voice cluster (classes 0..12) covers Speech,
// Conversation, Narration, Babbling, Shout, Yell, Whispering, etc. AudioSet is
// English/Western-biased, so non-English speech frequently lands on
// "Conversation" or "Narration" rather than the literal "Speech" — all of which
// map to CategorySpeech, making language-agnostic "is this voice?" decisions
// robust.
type SoundCategory int

const (
	CategorySpeech     SoundCategory = iota // human speech, any language (0..12)
	CategoryHumanOther                      // laughter, crying, singing, footsteps, applause (13..66)
	CategoryAnimal                          // dogs, cats, birds, livestock (67..131)
	CategoryMusic                           // music + instruments (132..282)
	CategoryNature                          // wind, water, thunder, fire (283..299)
	CategoryOther                           // vehicles, alarms, tools, glass, Silence, ... (rest)
)

// String returns a lowercase name for the category.
func (c SoundCategory) String() string {
	switch c {
	case CategorySpeech:
		return "speech"
	case CategoryHumanOther:
		return "humanOther"
	case CategoryAnimal:
		return "animal"
	case CategoryMusic:
		return "music"
	case CategoryNature:
		return "nature"
	default:
		return "other"
	}
}

// CategoryForYAMNetClass maps a YAMNet AudioSet class index (as carried in an
// EventSoundClass event's ReasonMask) to a broad SoundCategory. Out-of-range
// indices map to CategoryOther.
func CategoryForYAMNetClass(class int) SoundCategory {
	switch {
	case class >= 0 && class <= 12:
		return CategorySpeech
	case class >= 13 && class <= 66:
		return CategoryHumanOther
	case class >= 67 && class <= 131:
		return CategoryAnimal
	case class >= 132 && class <= 282:
		return CategoryMusic
	case class >= 283 && class <= 299:
		return CategoryNature
	default:
		return CategoryOther
	}
}

// IsSpeechClass reports whether a YAMNet class is human speech (in any
// language) — the speech cluster 0..12.
func IsSpeechClass(class int) bool {
	return CategoryForYAMNetClass(class) == CategorySpeech
}

// IsHumanVoice reports whether a YAMNet class is human vocal output: speech in
// any language plus laughter, crying, and singing (classes 0..34). This
// excludes non-vocal human sounds (whistling, breathing, footsteps) and
// everything non-human, so it answers "did a person's voice make this sound?"
func IsHumanVoice(class int) bool {
	return class >= 0 && class <= 34
}
