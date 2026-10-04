"""The exception raised for a failed native call."""

from auris.status import Status


class AurisError(RuntimeError):
    """A non-OK status from the native library; ``status`` says which."""

    def __init__(self, op: str, status: int, detail: str = "") -> None:
        self.op = op
        self.status = Status.of(status)
        name = self.status.name if isinstance(self.status, Status) else "UNKNOWN"
        suffix = f": {detail}" if detail else ""
        super().__init__(f"auris: {op} failed: {name} ({status}){suffix}")
