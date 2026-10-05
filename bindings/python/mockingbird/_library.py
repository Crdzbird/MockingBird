"""Locating, loading and binding libmockingbird."""

from __future__ import annotations

import ctypes
import os
import sys
import threading
from pathlib import Path

from mockingbird._signatures import SIGNATURES, EnginePtr
from mockingbird.error import MockingbirdError
from mockingbird.status import Status

LIBRARY_ENV = "MOCKINGBIRD_LIBRARY_PATH"
_lib: ctypes.CDLL | None = None
_lock = threading.Lock()


def _candidate_names() -> list[str]:
    if sys.platform == "darwin":
        return ["libmockingbird.dylib", "libmockingbird.0.1.0.dylib"]
    if sys.platform == "win32":
        return ["mockingbird.dll", "libmockingbird.dll"]
    return ["libmockingbird.so", "libmockingbird.so.0.1.0"]


def _candidates() -> list[Path]:
    repo_build = Path(__file__).resolve().parents[3] / "build"
    override = os.environ.get(LIBRARY_ENV)
    found = [Path(override)] if override else []
    found += [repo_build / n for n in _candidate_names()]
    return found + [Path(n) for n in _candidate_names()]


def _open(path: str | os.PathLike[str] | None) -> ctypes.CDLL:
    tried: list[str] = []
    for cand in [Path(path)] if path is not None else _candidates():
        try:
            lib = ctypes.CDLL(str(cand))
        except OSError:
            tried.append(str(cand))
            continue
        for name, argtypes, restype in SIGNATURES:
            fn = getattr(lib, name)
            fn.argtypes, fn.restype = argtypes, restype
        return lib
    raise OSError(
        "could not load libmockingbird; build it with `cmake -S . -B build && cmake --build build`"
        f" or set {LIBRARY_ENV}. Tried:\n  " + "\n  ".join(tried)
    )


def load_library(path: str | os.PathLike[str] | None = None) -> None:
    """Load libmockingbird from ``path`` (or the default search) before any Engine."""
    global _lib
    with _lock:
        _lib = _open(path)


def library() -> ctypes.CDLL:
    """The bound library, loaded on first use."""
    global _lib
    with _lock:
        if _lib is None:
            _lib = _open(None)
        return _lib


def version() -> str:
    """The ``X.Y.Z`` version of the linked libmockingbird."""
    return library().mockingbird_version_string().decode()


def status_name(status: int) -> str:
    """The library's own name for a status code."""
    return library().mockingbird_status_str(int(status)).decode()


def check(code: int, op: str) -> None:
    """Raise :class:`MockingbirdError` unless ``code`` is OK."""
    if code != Status.OK:
        raise MockingbirdError(op, code, library().mockingbird_status_str(code).decode())
