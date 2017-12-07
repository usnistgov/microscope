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

port = "5556"
if len(sys.argv) > 1:
    port =  sys.argv[1]
    int(port)

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind("tcp://*:%s" % port)

t = np.arange(-100, 900, dtype=float)
pulse = 30000*(np.exp(-t/200.) - np.exp(-t/40)) + 0
pulse[t<0] = 0
messagedata = np.asarray(pulse, dtype=np.uint16)

while True:
    channel = random.randrange(1,21)
    thisdata = 1000*channel + messagedata
    message = message_definition.DastardPulse(channel, 256, thisdata).pack()
    print "chan %d message length %d" % (channel, len(message))
    socket.send(message)
    time.sleep(0.1)
