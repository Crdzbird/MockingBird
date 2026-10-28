"""Wraps 16-bit little-endian PCM in a RIFF WAVE header."""

import struct

WAV_HEADER_BYTES = 44


def wav_from_pcm16(pcm: bytes, sample_rate_hz: int, channels: int = 1) -> bytes:
    """Return a complete ``.wav`` file holding ``pcm``.

    Concatenate one segment's :attr:`Codec.PCM_S16LE` payloads in drain order
    and pass them here to get a file any player opens.
    """
    header = struct.pack(
        "<4sI8sIHHIIHH4sI",
        b"RIFF",
        36 + len(pcm),
        b"WAVEfmt ",
        16,
        1,
        channels,
        sample_rate_hz,
        sample_rate_hz * channels * 2,
        channels * 2,
        16,
        b"data",
        len(pcm),
    )
    return header + bytes(pcm)
