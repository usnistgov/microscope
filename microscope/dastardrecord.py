from __future__ import annotations
import numpy as np
import struct
from collections import deque
from dataclasses import dataclass, replace
from functools import cache


@cache
def windowFunction(N: int) -> np.ndarray:
    return np.hanning(N)


@cache
def FFTFreq(N: int, dt: int | float) -> np.ndarray:
    return np.fft.rfftfreq(N, d=dt)


@dataclass(frozen=True)
class DastardRecord:
    channelIndex: int
    datatype: type
    nPresamples: int
    nSamples: int
    timebase: float
    voltsPerArb: float
    triggerTime: np.uint64
    frameIndex: np.uint16
    record: np.ndarray

    header_fmt = "<HBBIIffQQ"
    data_fmt = ("b", "B", "<h", "<H", "<i", "<I", "<q", "<Q")
    data_type = (np.int8, np.uint8, np.int16, np.uint16, np.int32, np.uint32, np.int64, np.uint64)

    @property
    def record_baseline_subtracted(self) -> np.ndarray:
        return self.record - self.record[:self.nPresamples - 1].mean()

    @classmethod
    def fromBinary(cls, header: bytes, contents: bytes) -> DastardRecord:
        values = struct.unpack(cls.header_fmt, header)
        chanidx, version, typecode, nPresamples, nSamples, timebase, voltsPerArb, triggerTime, frameIndex = values
        assert version == 0
        data_fmt = cls.data_fmt[typecode]
        datatype = cls.data_type[typecode]
        data = np.frombuffer(contents, dtype=data_fmt)

        return DastardRecord(chanidx, datatype, nPresamples, nSamples, timebase, voltsPerArb,
                             triggerTime, frameIndex, data)

    def PSD(self) -> np.ndarray:
        window = windowFunction(self.nSamples)
        fft = np.fft.rfft(window * (self.record - self.record.mean()))
        return np.abs(fft**2).real

    def FFTFreq(self) -> np.ndarray:
        return FFTFreq(self.nSamples, self.timebase)


def meanDastardRecord(buffer: deque[DastardRecord]) -> DastardRecord:
    n = len(buffer)
    assert n > 0

    if n == 1:
        return buffer[0]

    raw = np.zeros_like(buffer[0].record, dtype=float)
    for dr in buffer:
        raw += dr.record
    return replace(buffer[0], record=raw / n)
