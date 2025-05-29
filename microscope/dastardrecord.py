from __future__ import annotations
from typing import TypeVar, Generic
import numpy as np
import struct
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
    def record_baseline_subtracted(self) -> DastardRecord:
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
        fft = np.fft.rfft(window*(self.record-self.record.mean()))
        return np.abs(fft**2).real

    def FFTFreq(self) -> np.ndarray:
        return FFTFreq(self.nSamples, self.timebase)


T = TypeVar('T')


class ListBasedBuffer(Generic[T]):
    def __init__(self, capacity: int) -> None:
        self.capacity = capacity
        self.buffer: list[T] = []

    def __len__(self) -> int:
        return len(self.buffer)

    def push(self, x: T) -> None:
        if len(self.buffer) >= self.capacity:
            nextra = len(self.buffer) - self.capacity + 1
            self.buffer = self.buffer[nextra:]
        self.buffer.append(x)

    def last(self) -> T:
        return self.buffer[-1]

    def clear(self) -> None:
        self.buffer = []

    def resize(self, s: int) -> None:
        self.capacity = s
        if len(self.buffer) > self.capacity:
            nextra = len(self.buffer) - self.capacity
            self.buffer = self.buffer[nextra:]


class DastardRecordsBuffer(ListBasedBuffer):
    def mean(self) -> DastardRecordsBuffer:
        n = len(self.buffer)
        assert n > 0

        if n == 1:
            return self.buffer[0]

        r = replace(self.buffer[0])
        raw = np.asarray(r.record, dtype=float)
        for dr in self.buffer[1:]:
            raw += dr.record
        return replace(r, record=raw / n)
