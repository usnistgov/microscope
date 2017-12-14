#!/usr/bin/env python
"""
demo_pulses.py

Generate pulse-like data to appear as ZMQ packets, in order to develop microscope.
"""

import pylab as plt
import numpy as np
import zmq

import random
import sys
import time
import message_definition

chanmin, chanmax = 1, 25
samples, presamples = 1000, 200

port = "5556"
if len(sys.argv) > 1:
    port =  sys.argv[1]
    int(port)

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind("tcp://*:%s" % port)

pulseRecord = {ch:message_definition.DastardPulse(ch, presamples, 2.5e-6, 1./65535) for ch in range(chanmin, chanmax)}

while True:
    channel = random.randrange(1,21)
    noise = np.random.standard_normal(2+samples)*200

    thisdata = np.asarray(10000+1000*channel + noise[:-2]+2*noise[1:-1]+noise[2:], dtype=np.uint16)
    message = pulseRecord[channel].pack(thisdata)
    print "chan %d message length %d" % (channel, len(message))
    socket.send(message)
    time.sleep(0.1)
