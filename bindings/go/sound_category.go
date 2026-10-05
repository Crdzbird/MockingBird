package mockingbird

// SoundCategory folds YAMNet's 521 AudioSet classes into broad groups.
// AudioSet is English-biased: non-English speech often lands on
// "Conversation" or "Narration", and all of those map to CategorySpeech.
type SoundCategory int

const (
	// CategorySpeech is human speech in any language (classes 0..12).
	CategorySpeech SoundCategory = iota
	// CategoryHumanOther is laughter, crying, singing, footsteps (13..66).
	CategoryHumanOther
	// CategoryAnimal covers classes 67..131.
	CategoryAnimal
	// CategoryMusic covers classes 132..282.
	CategoryMusic
	// CategoryNature is wind, water, thunder, fire (283..299).
	CategoryNature
	// CategoryOther is vehicles, alarms, tools, glass and silence (494).
	CategoryOther
)

// String returns the lowercase category name.
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
	}
	return "other"
}

// SoundCategoryOf maps a YAMNet AudioSet class index to its category.
func SoundCategoryOf(class int) SoundCategory {
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
	}
	return CategoryOther
}

// IsHumanVoice reports whether a YAMNet class is a person's voice in any
// language: speech, laughter, crying and singing (classes 0..34).
func IsHumanVoice(class int) bool { return class >= 0 && class <= 34 }
