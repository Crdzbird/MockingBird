"""How segment audio is encoded before it reaches the host."""

from __future__ import annotations

from dataclasses import dataclass

from mockingbird.codec import Codec


@dataclass(slots=True)
class EncoderConfig:
    """PCM passthrough by default, with Opus settings ready to use."""

    codec: Codec = Codec.PCM_S16LE
    opus_bitrate_bps: int = 32000
    opus_vbr: bool = True
    opus_complexity: int = 5
    """0..10; higher is better quality for more CPU."""

    @classmethod
    def opus(cls) -> EncoderConfig:
        """Opus at 32 kbps VBR, the usual choice for live voice."""
        return cls(codec=Codec.OPUS)
