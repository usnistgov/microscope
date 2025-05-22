# Qt5 imports
from PyQt5 import QtCore

# Non-Qt imports
import numpy as np
import struct
import zmq

from dastardrecord import DastardRecord


class ZMQSubscriber(QtCore.QObject):
    """Code suggested by https://wiki.python.org/moin/PyQt/Writing%20a%20client%20for%20a%20zeromq%20service"""

    pulserecord = QtCore.pyqtSignal(DastardRecord)

    def __init__(self, host, port):
        QtCore.QObject.__init__(self)

        # Socket to talk to server
        context = zmq.Context()
        self.socket = context.socket(zmq.SUB)

        self.host = host
        self.baseport = port
        self.address = f"tcp://{self.host}:{self.baseport}"
        self.socket.connect(self.address)
        print(f"Collecting updates from dastard at {self.address}")

        self.socket.setsockopt_string(zmq.SUBSCRIBE, "")

        # self.messages_seen = collections.Counter()
        self.quit_once = False
        self.running = False

    def data_monitor_loop(self):
        if self.quit_once:
            raise ValueError("Cannot run a ZMQListener.loop more than once!")
        self.running = True
        while self.running:
            # Check socket for events, with 100 ms timeout (so this loop and
            # its thread can end quickly when self.running set to False)
            if self.socket.poll(100) == 0:
                continue

            msg = self.socket.recv_multipart()
            try:
                header, contents = msg
            except (ValueError, TypeError):
                raise Exception(f"msg: `{msg}` should have two parts, but does not")
            try:
                record = DastardRecord.fromBinary(header, contents)
            except Exception as e:
                print(f"Error processing pulse record is: {e}")
                return

            self.pulserecord.emit(record)

        self.socket.close()
        self.quit_once = True
        print("ZMQListener quit cleanly")
