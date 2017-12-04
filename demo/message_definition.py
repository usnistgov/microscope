import struct

class DastardPulse(object):
    """Represent a single pulse record from DASTARD"""

    def __init__(self, channel, presamples, data):
        self.channel = channel
        self.presamples = presamples
        self.data = data

    def pack(self):
        fmt = "<ll%dH"%len(self.data)
        return struct.pack(fmt, self.channel, self.presamples, self.data)
