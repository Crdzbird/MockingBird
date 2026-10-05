"""One encoded audio packet."""

from __future__ import annotations

from dataclasses import dataclass

from mockingbird._packet_struct import PacketStruct
from mockingbird.codec import Codec
from mockingbird.packet_flag import PacketFlag


@dataclass(frozen=True, slots=True)
class Packet:
    """``data`` is a Python-owned copy, safe to keep and forward."""

    segment_id: int
    pts_us: int
    capture_ts_us: int
    flags: int
    codec: Codec | int
    data: bytes

    @classmethod
    def from_struct(cls, p: PacketStruct) -> Packet:
        """Copy a native ``mockingbird_packet_t`` out of the engine's pool."""
        data = bytes(p.data[: p.size]) if p.size and p.data else b""
        codec = Codec(p.codec) if p.codec in Codec._value2member_map_ else p.codec
        return cls(p.segment_id, p.pts_us, p.capture_ts_us, p.flags, codec, data)

    @property
    def is_preroll(self) -> bool:
        """Captured before the trigger."""
        return bool(self.flags & PacketFlag.PREROLL)

    @property
    def is_postroll(self) -> bool:
        """Trailing audio after the detector closed."""
        return bool(self.flags & PacketFlag.POSTROLL)

    @property
    def is_keyframe(self) -> bool:
        """First encoded frame of its segment."""
        return bool(self.flags & PacketFlag.KEYFRAME)

    @property
    def is_last(self) -> bool:
        """Terminal packet of its segment."""
        return bool(self.flags & PacketFlag.LAST)
