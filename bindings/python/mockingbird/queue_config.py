"""The queues between capture, the worker and the host."""

from dataclasses import dataclass


@dataclass(slots=True)
class QueueConfig:
    """Ring sizes and the ungated PCM tap."""

    pcm_ring_frames: int = 1 << 15
    """Capture ring capacity in samples, two seconds at 16 kHz."""
    packet_queue_capacity: int = 256
    """Host packet queue depth; the oldest packet drops first."""
    pcm_tap: bool = False
    """Also expose every processed frame through :meth:`Engine.read_pcm`."""
    pcm_tap_frames: int = 0
    """Tap ring capacity; 0 is one second of audio."""
