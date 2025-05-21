#!/usr/bin/env python3

"""
microscope

A GUI client to monitor microcalorimeter pulses produced by
the DASTARD server (Data Acquisition System for Triggering, Analyzing, and Recording Data).

By Joe Fowler and Galen O'Neil
NIST Boulder Laboratories
May 2025 -

Original C++ version May 2018-May 2025
"""

# Qt5 imports
import PyQt5.uic
from PyQt5 import QtGui, QtWidgets
from PyQt5.QtCore import QSettings, pyqtSlot, QCoreApplication
# from PyQt5.QtWidgets import QFileDialog

# Non-Qt imports
import argparse
import json
import pathlib
# import subprocess
import sys
import os
# import yaml
# import zmq

# from collections import defaultdict
# import numpy as np

# # User code imports
import plotwindow
from channel_group import ChannelGroup
# from . import disable_hyperactive
# from . import rpc_client
# from . import status_monitor
# from . import special_channels
# from . import trigger_config
# from . import trigger_config_simple
# from . import writing
# from . import projectors
# from . import observe
# from . import workflow

try:
    from _version import version as __version__
    from _version import version_tuple
except ImportError:
    __version__ = "unknown version"
    version_tuple = (0, 0, "unknown version")


QCoreApplication.setOrganizationName("Quantum Sensors Group")
QCoreApplication.setOrganizationDomain("nist.gov")
QCoreApplication.setApplicationName("Microscope")


class MainWindow(QtWidgets.QMainWindow):  # noqa: PLR0904
    def __init__(self, settings, title, isTDM, parent=None):
        self.settings = settings
        self.isTDM = isTDM

        QtWidgets.QMainWindow.__init__(self, parent)
        my_dir = os.path.dirname(__file__)
        self.setWindowIcon(QtGui.QIcon(os.path.join(my_dir, "ui/dc.png")))
        PyQt5.uic.loadUi(os.path.join(my_dir, "ui/microscope.ui"), self)
        self.setWindowTitle(title)

        self.plotWindows = []
        pw1 = plotwindow.PlotWindow(None, isTDM=isTDM)
        self.plotWindows.append(pw1)
        self.frame.layout().addWidget(pw1, 0, 0)

    @pyqtSlot()
    def savePlot(self): pass

    # @pyqtSlot()
    # def close(self):
    #     """Cleanly close the zmqlistener and block certain signals in the
    #     trigger config widget."""
    #     # self.triggerTab._closing()
    #     # self.zmqlistener.running = False
    #     # self.zmqthread.quit()
    #     # self.zmqthread.wait()
    #     # self.observeWindow.hide()  # prevents close hanging due to still visible observeWindow
    #     self.c


def version_message():
    return f"This is microscope version {__version__}"


def read_channels_json_file(args):
    try:
        HOME = pathlib.Path.home()
        with open(f"{HOME}/.dastard/channels.json", "r", encoding="ascii") as fp:
            info = json.load(fp)
        args.nsensors = 0
        args.channel_groups = []
        for d in info:
            cg = ChannelGroup(d["Firstchan"], d["Nchan"])
            args.channel_groups.append(cg)
            args.nsensors += cg.nChan
        args.nchan = args.nsensors
        if args.tdm:
            args.nchan *= 2

    except Exception:
        print("Could not read the channel file $HOME/.dastard/channels.json")
        print("Therefore, you must set row+column counts with -rNR -cNC, or use indexing with -i.")
        raise ValueError("could not read ~/.dastard/channels.json")


def parsed_arguments():
    parser = argparse.ArgumentParser(
        prog='Microscope',
        description='A GUI for plotting microcalorimeter pulses live'
    )
    parser.add_argument("-a", "--appname", default="Microscope: microcalorimeter plots")
    parser.add_argument("-r", "--rows", type=int, default=0)
    parser.add_argument("-c", "--columns", type=int, default=0)
    parser.add_argument("-N", "--nsensors", default=0)
    parser.add_argument("-i", "--indexing", action="store_true")
    parser.add_argument("-n", "--no-error-channel", action="store_true")
    parser.add_argument("-v", "--version", action="store_true")
    args = parser.parse_args()
    args.tdm = not args.no_error_channel
    print(version_message())
    if args.version:
        sys.exit(0)

    args.channel_groups = []

    # There are 3 ways to number channels. We can:
    # 1. use indexing,
    # 2. set rows+cols, or
    # 3. read the config file $HOME/.dastard/channels.json to learn the channel groups.
    if args.indexing:
        if args.nsensors <= 0:
            DEFAULT_NSENSORS = 256
            args.nsensors = DEFAULT_NSENSORS
        args.nchan = args.nsensors
        if args.tdm:
            args.nchan *= 2
        cg = ChannelGroup(1, args.nsensors)
        args.channel_groups.append(cg)
        return args

    if args.rows > 0 and args.columns > 0:
        # If -r and -c arguments are nonzero, treat each column as a channel group with nrows channels,
        # (or double this if it's a TDM system).
        # These arguments override reading the $HOME/.dastard/channels.json file.
        chanpercol = args.rows
        if args.tdm:
            chanpercol *= 2
        args.nsensors = args.rows * args.columns
        args.nchan = chanpercol * args.columns
        fc = 0
        for i in range(args.columns):
            cg = ChannelGroup(fc, chanpercol)
            args.channel_groups.append(cg)
            fc += chanpercol
        return args

    if args.rows > 0 or args.columns > 0:
        msg = f"Command-line rows={args.rows} and columns={args.columns}. Must set both nonzero"
        raise ValueError(msg)

    read_channels_json_file(args)
    return args


def main():
    args = parsed_arguments()

    settings = QSettings("NIST Quantum Sensors", "Microscope")

    app = QtWidgets.QApplication(sys.argv)
    app.setWindowIcon(QtGui.QIcon("../ui/microscope.png"))
    app.setApplicationName(args.appname)
    app.setApplicationDisplayName(args.appname)

    short_ver = __version__.split("+")[0]
    title = f"{args.appname} (version {short_ver})"
    window = MainWindow(settings, title, args.tdm)

    window.show()
    retval = app.exec_()
    sys.exit(retval)


if __name__ == "__main__":
    main()
