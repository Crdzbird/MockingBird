"""The exception raised for a failed native call."""

from mockingbird.status import Status


class MockingbirdError(RuntimeError):
    """A non-OK status from the native library; ``status`` says which."""

    def __init__(self, op: str, status: int, detail: str = "") -> None:
        self.op = op
        self.status = Status.of(status)
        name = self.status.name if isinstance(self.status, Status) else "UNKNOWN"
        suffix = f": {detail}" if detail else ""
        super().__init__(f"mockingbird: {op} failed: {name} ({status}){suffix}")
