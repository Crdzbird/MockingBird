"""The native handle shared by the Engine's mixins."""

from __future__ import annotations

import ctypes

from auris._library import EnginePtr, check, library
from auris.error import AurisError
from auris.status import Status


class EngineHandle:
    """Owns the ``auris_engine_t`` pointer and the open/running flags."""

    _lib: ctypes.CDLL
    _handle: ctypes.c_void_p
    _closed: bool
    _running: bool

    def _bind(self) -> None:
        self._lib = library()
        self._handle = EnginePtr()
        self._closed = False
        self._running = False

    def _require_open(self) -> None:
        if self._closed:
            raise AurisError("engine", Status.ERR_INVALID_STATE, "engine is closed")

    def _call(self, name: str, *args: object) -> int:
        """Invoke a native entry point on this handle, raising on failure."""
        self._require_open()
        code = getattr(self._lib, name)(self._handle, *args)
        check(code, name)
        return code
