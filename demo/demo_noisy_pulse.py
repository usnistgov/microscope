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
    port =  sys.argv[1]
    int(port)

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind("tcp://*:%s" % port)

t = np.arange(1-presamples, samples-presamples+1, dtype=float)
pulse = (np.exp(-t/200.) - np.exp(-t/40)) + 0
pulse[t<0] = 0
messagedata = {ch:np.asarray(pulse*(ch+20)*1000+1000*ch, dtype=np.uint16) for ch in range(chanmin, chanmax+1)}
pulseRecord = {ch:message_definition.DastardPulse(ch, presamples, 2.5e-6, 1./65535) for ch in range(chanmin, chanmax+1)}

while True:
    channel = random.randrange(chanmin, chanmax+1)
    thisdata = np.asarray(messagedata[channel] + np.random.random_integers(-500, 500, size=samples), dtype=np.uint16)
    header = pulseRecord[channel].packheader(thisdata)
    print("chan %d message length %d" % (channel, len(thisdata)))
    socket.send(header, zmq.SNDMORE)
    socket.send(thisdata.data[:])
    time.sleep(0.1)
