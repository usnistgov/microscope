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

chanmin, chanmax = 1, 24
samples, presamples = 4000, 200
noiserms = 100.0
timebase = 2.5e-6

port = "5502"
if len(sys.argv) > 1:
    port = sys.argv[1]
    int(port)

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind(f"tcp://*:{port}")

rng = np.random.default_rng()
dt = 1. / 65535
pulseRecord = {ch: message_definition.DastardPulse(ch - chanmin, presamples, timebase, dt)
               for ch in range(chanmin, 1 + chanmax)}

while True:
    channel = random.randrange(chanmin, 1 + chanmax)
    noise = rng.standard_normal(2 + samples)
    lowpassnoise = (noise[:-2] + 2 * noise[1:-1] + noise[2:]) * 0.25 * noiserms

    thisdata = np.asarray(10000 + 1000 * channel + lowpassnoise, dtype=np.uint16)
    header = pulseRecord[channel].packheader(thisdata)
    print(f"chan {channel:3d} message length {len(thisdata)}")
    socket.send(header, zmq.SNDMORE)
    socket.send(thisdata.data[:])
    time.sleep(0.1)
