import numpy as np
import struct
from dataclasses import dataclass, replace


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
    def record_baseline_subtracted(self):
        return self.record - self.record[:self.nPresamples-1].mean()

    @classmethod
    def fromBinary(cls, header, contents):
        values = struct.unpack(cls.header_fmt, header)
        chanidx, version, typecode, nPresamples, nSamples, timebase, voltsPerArb, triggerTime, frameIndex = values
        assert version == 0
        data_fmt = cls.data_fmt[typecode]
        datatype = cls.data_type[typecode]
        data = np.frombuffer(contents, dtype=data_fmt)

        return DastardRecord(chanidx, datatype, nPresamples, nSamples, timebase, voltsPerArb,
                             triggerTime, frameIndex, data)


class DastardRecordsBuffer:
    def __init__(self, capacity):
        self.capacity = capacity
        self.buffer = []

    def __len__(self):
        return len(self.buffer)

    def push(self, x):
        if len(self.buffer) >= self.capacity:
            nextra = len(self.buffer) - self.capacity + 1
            self.buffer = self.buffer[nextra:]
        self.buffer.append(x)

    def clear(self):
        self.buffer = []

    def resize(self, s):
        self.capacity = s
        if len(self.buffer) > self.capacity:
            nextra = len(self.buffer) - self.capacity
            self.buffer = self.buffer[nextra:]

    def mean(self):
        n = len(self.buffer)
        if n <= 0:
            return None

        if n == 1:
            return self.buffer[0]

        r = replace(self.buffer[0])
        raw = np.asarray(r.record, dtype=float)
        for dr in self.buffer[1:]:
            raw += dr.record
        return replace(r, record=raw/n)
