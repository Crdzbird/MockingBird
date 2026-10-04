"""A packet's place in its segment."""

from enum import IntFlag


class PacketFlag(IntFlag):
    """Bits of :attr:`Packet.flags`."""

    PREROLL = 1
    POSTROLL = 2
    KEYFRAME = 4
    LAST = 8
