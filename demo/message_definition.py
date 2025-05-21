import struct
import time
import numpy as np


class DastardPulse:
    """Represent a single pulse record from DASTARD"""

    version = 0

    def __init__(self, channel, presamples, sampletime, voltsperarb):
        self.__dict__.update(locals())
        self.serialnumber = 0

    def packheader(self, data, trig_time=None, serialnumber=None):
        if data.dtype in {np.int64, np.uint64}:
            wordcode, size = 7, 8
        elif data.dtype in {np.int32, np.uint32}:
            wordcode, size = 5, 4
        elif data.dtype == np.uint16:
            wordcode, size = 3, 2
        elif data.dtype == np.int16:
            wordcode = 2
            size = 2
        elif data.dtype in {np.int8, np.uint8}:
            wordcode, size = 1, 1
        else:
            raise ValueError("Cannot handle numpy type {data.dtype}")

        if trig_time is None:
            trig_time = np.uint64(time.time() * 1e9)
        if serialnumber is None:
            serialnumber = self.serialnumber
        self.serialnumber += size

        fmt = "<HbbllffQQ"
        header = struct.pack(fmt, self.channel, self.version, wordcode,
                             self.presamples, len(data),
                             self.sampletime, self.voltsperarb,
                             trig_time, serialnumber)
        return header
