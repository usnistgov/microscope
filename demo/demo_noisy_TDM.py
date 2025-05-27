#!/usr/bin/env python
"""
demo_pulses.py

Generate pulse-like data to appear as ZMQ packets, in order to develop microscope.
"""

import numpy as np
import zmq

import random
import sys
import time
import message_definition

chanmin, chanmax = 0, 11
samples, presamples = 1000, 200

port = "5502"
if len(sys.argv) > 1:
    port = sys.argv[1]
    int(port)

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind(f"tcp://*:{port}")

rng = np.random.default_rng()
t = np.arange(1 - presamples, samples - presamples + 1, dtype=float)
pulse = (np.exp(-t / 200.) - np.exp(-t / 40)) + 0
pulse[t < 0] = 0
derivative = np.roll(pulse, 1) - pulse
derivative[0] = 0
messagedata = {}
pulseRecord = {}
for chnum in range(chanmin, chanmax + 1):
    errindex = 2 * (chnum - chanmin)
    fbindex = errindex + 1
    messagedata[fbindex] = np.asarray(pulse * (chnum + 20) * 1000 + 1000 * chnum, dtype=np.uint16)
    messagedata[errindex] = np.asarray(derivative * (chnum + 20) * 1000 + 1000 * chnum, dtype=np.uint16)
    pulseRecord[fbindex] = message_definition.DastardPulse(fbindex, presamples, 2.5e-6, 1. / 65535)
    pulseRecord[errindex] = message_definition.DastardPulse(errindex, presamples, 2.5e-6, 1. / 65535)

while True:
    chnum = random.randrange(chanmin, chanmax + 1)
    errindex = 2 * (chnum - chanmin)
    fbindex = errindex + 1

    for chindex in (fbindex, errindex):
        thisdata = np.asarray(messagedata[chindex] + rng.integers(-500, 500, size=samples), dtype=np.uint16)
        header = pulseRecord[chindex].packheader(thisdata)
        socket.send(header, zmq.SNDMORE)
        socket.send(thisdata.data[:])
    print(f"chan {chnum:3d} message length {len(thisdata)}")
    time.sleep(0.1)
