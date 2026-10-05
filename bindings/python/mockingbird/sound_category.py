"""Broad categories for YAMNet's 521 AudioSet classes."""

from enum import IntEnum


class SoundCategory(IntEnum):
    """AudioSet is English-biased: non-English speech often lands on
    "Conversation" or "Narration", and all of those map to :attr:`SPEECH`."""

    SPEECH = 0
    """Human speech in any language (classes 0..12)."""
    HUMAN_OTHER = 1
    """Laughter, crying, singing, breathing, footsteps (13..66)."""
    ANIMAL = 2
    """Classes 67..131."""
    MUSIC = 3
    """Classes 132..282."""
    NATURE = 4
    """Wind, water, thunder, fire (283..299)."""
    OTHER = 5
    """Vehicles, alarms, tools, glass and silence (494)."""

    def __str__(self) -> str:
        return self.name.lower()

    @classmethod
    def of(cls, class_id: int) -> "SoundCategory":
        """The category of a YAMNet AudioSet class index."""
        match class_id:
            case n if 0 <= n <= 12:
                return cls.SPEECH
            case n if 13 <= n <= 66:
                return cls.HUMAN_OTHER
            case n if 67 <= n <= 131:
                return cls.ANIMAL
            case n if 132 <= n <= 282:
                return cls.MUSIC
            case n if 283 <= n <= 299:
                return cls.NATURE
            case _:
                return cls.OTHER

    @staticmethod
    def is_human_voice(class_id: int) -> bool:
        """True for a person's voice in any language: speech, laughter, crying
        and singing (classes 0..34)."""
        return 0 <= class_id <= 34
