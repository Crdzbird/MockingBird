"""One native engine."""

from __future__ import annotations

import ctypes
from typing import Any

from mockingbird._config_struct import ConfigStruct
from mockingbird._engine_lifecycle import EngineLifecycle
from mockingbird._engine_output import EngineOutput
from mockingbird._library import check
from mockingbird._marshal import to_struct
from mockingbird._pcm_buffer import as_int16_buffer
from mockingbird.config import Config


class Engine(EngineLifecycle, EngineOutput):
    """Create it, :meth:`start` it, drain :meth:`poll_events` and
    :meth:`drain_packets`, and :meth:`close` it; a ``with`` block does the
    last part for you. Drive it from one thread."""

    def __init__(self, config: Config | None = None) -> None:
        self._bind()
        self._config = config or Config()
        c = self._marshal(self._config)
        check(self._lib.mockingbird_create(ctypes.byref(c), ctypes.byref(self._handle)), "mockingbird_create")

    @property
    def config(self) -> Config:
        """The configuration in effect, including live retunes."""
        return self._config

    def push_pcm(self, samples: Any, capture_ts_us: int = 0) -> None:
        """Feed int16 mono PCM into a ``HOST_PCM`` engine. Accepts bytes,
        ``array('h')``, numpy int16 arrays, memoryviews or sequences of ints."""
        buf, count = as_int16_buffer(samples)
        if count:
            self._call("mockingbird_push_pcm", buf, count, capture_ts_us)

    def update_config(self, config: Config) -> None:
        """Retune in place: detector toggles and thresholds, the silence gate,
        degradation and wake batching. Detector state survives; structural
        fields must match the creating config."""
        c = self._marshal(config)
        self._call("mockingbird_update_config", ctypes.byref(c))
        self._config = config

    def _marshal(self, config: Config) -> ConfigStruct:
        defaults = ConfigStruct()
        check(self._lib.mockingbird_config_defaults(ctypes.byref(defaults)), "mockingbird_config_defaults")
        return to_struct(config, defaults)
