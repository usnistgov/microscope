import struct
import numpy as np

class DastardPulse(object):
    """Represent a single pulse record from DASTARD"""

    def __init__(self, channel, presamples, data):
        self.channel = channel
        self.presamples = presamples
        self.data = data

    def pack(self):
        if self.data.dtype in (np.int64, np.uint64):
            wordcode, size = "Q", 8
        elif self.data.dtype in (np.int32, np.uint32):
            wordcode, size = "L", 4
        elif self.data.dtype in (np.int16, np.uint16):
            wordcode, size = "H", 2
        elif self.data.dtype in (np.int8, np.uint8):
            wordcode, size = "B", 1
        else:
            raise ValueError("Cannot handle numpy type %s"%self.data.dtype)
        fmt = "<lll%d%s"%(len(self.data), wordcode)
        return struct.pack(fmt, self.channel, self.presamples, size, *self.data)
