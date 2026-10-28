"""The microphones an engine can open."""

from __future__ import annotations

import ctypes
from dataclasses import dataclass

from mockingbird._device_struct import DeviceStruct
from mockingbird._library import check, library
from mockingbird.backend import Backend


@dataclass(frozen=True, slots=True)
class InputDevice:
    """A microphone the system offers."""

    id: str
    """What :meth:`Engine.select_input_device` takes."""
    name: str
    is_default: bool


def input_devices(backend: Backend = Backend.AUTO) -> list[InputDevice]:
    """List the inputs ``backend`` can open. Raises :class:`MockingbirdError`
    with ``UNSUPPORTED`` where the platform has no listing."""
    capacity = 64
    out = (DeviceStruct * capacity)()
    count = ctypes.c_uint32()
    check(
        library().mockingbird_list_input_devices(int(backend), out, capacity, ctypes.byref(count)),
        "mockingbird_list_input_devices",
    )
    return [
        InputDevice(
            d.id.decode("utf-8", "replace"),
            d.name.decode("utf-8", "replace"),
            bool(d.is_default),
        )
        for d in out[: min(count.value, capacity)]
    ]
