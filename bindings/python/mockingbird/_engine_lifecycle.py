"""Start, stop, close and the context-manager protocol."""

from __future__ import annotations

from mockingbird._engine_handle import EngineHandle
from mockingbird._library import EnginePtr


class EngineLifecycle(EngineHandle):
    """Mixed into :class:`Engine`."""

    def __enter__(self):
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def __del__(self) -> None:
        try:
            self.close()
        except Exception:
            pass

    @property
    def is_running(self) -> bool:
        """True between :meth:`start` and :meth:`stop`."""
        return self._running

    def start(self) -> None:
        """Begin capture and detection."""
        self._call("mockingbird_start")
        self._running = True

    def select_input_device(self, device_id: str | None) -> None:
        """Open ``device_id``, from :func:`input_devices`, on the next
        :meth:`start`; ``None`` follows the system default. Only while stopped."""
        self._call("mockingbird_set_input_device", device_id.encode("utf-8") if device_id else None)

    def stop(self) -> None:
        """Halt capture; queued packets stay drainable and the engine can restart."""
        self._call("mockingbird_stop")
        self._running = False

    def close(self) -> None:
        """Stop if needed and destroy the native engine. Idempotent."""
        if getattr(self, "_closed", True):
            return
        self._closed = True
        self._running = False
        if self._handle:
            self._lib.mockingbird_destroy(self._handle)
            self._handle = EnginePtr()
