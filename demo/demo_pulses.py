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

t = np.linspace(-256, 767, 1024)
pulse = 30000*(np.exp(-t/200.) - np.exp(-t/40)) + 1000
pulse[t<0] = 1000
messagedata = np.asarray(pulse, dtype=np.uint16)

while True:
    channel = random.randrange(1,8)
    message = message_definition.DastardPulse(channel, 256, messagedata)
    print "chan %d data: %s message length %d" % (channel, messagedata, len(message.pack()))
    socket.send(message.pack())
    time.sleep(1)
