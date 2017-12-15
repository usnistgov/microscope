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

t = np.arange(1-presamples, samples-presamples+1, dtype=float)
pulse = 30000*(np.exp(-t/200.) - np.exp(-t/40)) + 0
pulse[t<0] = 0
messagedata = np.asarray(pulse , dtype=np.uint16)
pulseRecord = {ch:message_definition.DastardPulse(ch, presamples, 2.5e-6, 1./65535) for ch in range(chanmin, chanmax)}

while True:
    channel = random.randrange(1,21)
    thisdata = np.asarray(1000*channel + messagedata + np.random.random_integers(-500, 500, size=samples), dtype=np.uint16)
    message = pulseRecord[channel].pack(thisdata)
    print "chan %d message length %d" % (channel, len(message))
    socket.send(message)
    time.sleep(0.1)