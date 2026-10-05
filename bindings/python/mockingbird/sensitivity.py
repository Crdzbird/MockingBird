"""One 0..1 dial that drives every detector's thresholds."""


class Sensitivity:
    """0 is the least sensitive; 1 catches faint and borderline sounds."""

    __slots__ = ("value",)

    def __init__(self, raw: float) -> None:
        self.value = min(1.0, max(0.0, float(raw)))

    def lerp(self, at_zero: float, at_one: float) -> float:
        """The threshold value at this sensitivity, from ``at_zero`` (least
        sensitive) to ``at_one`` (most sensitive)."""
        return at_zero + (at_one - at_zero) * self.value
