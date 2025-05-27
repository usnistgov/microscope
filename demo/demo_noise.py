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

chanmin, chanmax = 1, 25
samples, presamples = 1000, 200

port = "5502"
if len(sys.argv) > 1:
    port = sys.argv[1]
    int(port)

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind(f"tcp://*:{port}")

rng = np.random.default_rng()
pulseRecord = {ch: message_definition.DastardPulse(ch - chanmin, presamples, 2.5e-6, 1. / 65535) for ch in range(chanmin, chanmax)}

while True:
    channel = random.randrange(1, 21)
    noise = rng.standard_normal(2 + samples) * 200

    thisdata = np.asarray(10000 + 1000 * channel + noise[:-2] + 2 * noise[1:-1] + noise[2:], dtype=np.uint16)
    header = pulseRecord[channel].packheader(thisdata)
    print(f"chan {channel:3d} message length {len(thisdata)}")
    socket.send(header, zmq.SNDMORE)
    socket.send(thisdata.data[:])
    time.sleep(0.1)
