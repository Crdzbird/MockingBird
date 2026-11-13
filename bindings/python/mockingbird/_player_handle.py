"""The native player handle and its lifecycle, mixed into :class:`Player`."""

from __future__ import annotations

import ctypes

from mockingbird._library import check, library
from mockingbird._playback_signatures import PlayerPtr
from mockingbird.error import MockingbirdError
from mockingbird.status import Status


class PlayerHandle:
    """Owns the ``mockingbird_player_t`` pointer: start, pause, resume, stop,
    close and the context-manager protocol."""

    _lib: ctypes.CDLL
    _handle: ctypes.c_void_p
    _closed: bool

    def _bind(self) -> None:
        self._lib = library()
        self._handle = PlayerPtr()
        self._closed = False

    def _call(self, name: str, *args: object) -> None:
        """Invoke a native entry point on this handle, raising on failure."""
        if self._closed:
            raise MockingbirdError("player", Status.ERR_INVALID_STATE, "player is closed")
        check(getattr(self._lib, name)(self._handle, *args), name)

    def __enter__(self):
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass

    def start(self) -> None:
        """Open the output; ``ERR_ALREADY_RUNNING`` while it runs."""
        self._call("mockingbird_player_start")

    def pause(self) -> None:
        """Stop the output and keep what is buffered."""
        self._call("mockingbird_player_pause")

    def resume(self) -> None:
        """Start the output again after :meth:`pause` or a lost route."""
        self._call("mockingbird_player_resume")

    def stop(self) -> None:
        """Close the output and drop everything buffered; the player can restart."""
        self._call("mockingbird_player_stop")

    def close(self) -> None:
        """Stop if needed and destroy the native player. Idempotent."""
        if getattr(self, "_closed", True):
            return
        self._closed = True
        if self._handle:
            self._lib.mockingbird_player_stop(self._handle)
            self._lib.mockingbird_player_destroy(self._handle)
            self._handle = PlayerPtr()
