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

t = np.arange(-256, 767, 1024)
messagedata = np.exp(-t/200.) - np.exp(-t/40) + 1000
messagedata[t<0] = 1000


while True:
    channel = random.randrange(1,8)
    print "%d %d" % (channel, messagedata)
    message = message_definition.DastardPulse(channel, 256, messagedata)
    socket.send(message.pack())
    time.sleep(1)
