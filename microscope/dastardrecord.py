import numpy as np
import struct
from dataclasses import dataclass


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
