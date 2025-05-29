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
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import QSettings, pyqtSlot, QCoreApplication
import pyqtgraph as pg

# Non-Qt imports
import argparse
import json
import pathlib
import sys
import os

# # User code imports
import plotwindow
import subscriber
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


QCoreApplication.setOrganizationName("NIST Quantum Sensors")
QCoreApplication.setOrganizationDomain("nist.gov")
QCoreApplication.setApplicationName("Microscope")


class AboutDialog(QtWidgets.QDialog):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)

        self.setWindowTitle("About Microscope")

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(QtWidgets.QLabel("<b>Microscope</b>"))
        layout.addWidget(QtWidgets.QLabel(f"{parent.title} Python version"))
        layout.addWidget(QtWidgets.QLabel("by Joe Fowler and NIST Boulder Labs, 2025"))
        layout.addWidget(QtWidgets.QLabel("Not subject to copyright in the United States"))
        self.setLayout(layout)


class MainWindow(QtWidgets.QMainWindow):  # noqa: PLR0904
    def __init__(self, settings: QSettings, title: str, isTDM: bool, channel_groups: list[ChannelGroup],
                 host: str, port: int) -> None:
        self.settings = settings
        self.isTDM = isTDM
        self.channel_groups: list[ChannelGroup] = []
        self.channel_index: dict[int, int] = {}
        self.channel_number: dict[int, int] = {}
        self.set_channel_groups(channel_groups)

        parent = None
        QtWidgets.QMainWindow.__init__(self, parent)
        my_dir = os.path.dirname(__file__)
        self.setWindowIcon(QtGui.QIcon(os.path.join(my_dir, "ui/dc.png")))
        PyQt5.uic.loadUi(os.path.join(my_dir, "ui/microscope.ui"), self)
        self.setWindowTitle(title)
        self.title = title
        self.actionAbout.triggered.connect(self.show_about)

        self.statusLabel1 = QtWidgets.QLabel("")
        ss = "QLabel {{ color : black; size : 9}}"
        self.statusLabel1.setStyleSheet(ss)
        sb = self.statusBar()
        if sb is not None:
            sb.addWidget(self.statusLabel1)

        self.subscribedChannels: set[int] = set()
        self.zmqthread = QtCore.QThread()
        self.zmqsubscriber = subscriber.ZMQSubscriber(host, port)
        self.zmqsubscriber.moveToThread(self.zmqthread)
        # self.zmqsubscriber.pulserecord.connect(self.updateReceived)
        self.zmqthread.started.connect(self.zmqsubscriber.data_monitor_loop)
        QtCore.QTimer.singleShot(0, self.zmqthread.start)

        self.plotWindows: dict[tuple[int, int], plotwindow.PlotWindow] = {}
        self.addPlotWindow()
        self.addPlotsButton.clicked.connect(self.addPlotWindow)

    @pyqtSlot()
    def show_about(self) -> None:
        dialog = AboutDialog(self)
        dialog.show()

    @pyqtSlot()
    def addPlotWindow(self) -> None:
        if len(self.plotWindows) >= 4:
            return
        pw = plotwindow.PlotWindow(self, self.channel_groups, isTDM=self.isTDM)
        self.zmqsubscriber.pulserecord.connect(pw.updateReceived)
        pw.updateSubscriptions.connect(self.updateSubscriptions)
        for location in ((0, 0), (0, 1), (1, 0), (1, 1)):
            if location not in self.plotWindows:
                self.plotWindows[location] = pw
                r, c = location
                self.frame.layout().addWidget(pw, r, c)
                pw.closed.connect(self.removePlotWindow)
                if len(self.plotWindows) >= 4:
                    self.addPlotsButton.setDisabled(True)
                return

    @pyqtSlot()
    def removePlotWindow(self) -> None:
        sender = self.sender()
        for location in ((0, 0), (0, 1), (1, 0), (1, 1)):
            if self.plotWindows.get(location, None) == sender:
                del self.plotWindows[location]
                self.addPlotsButton.setEnabled(True)

    @pyqtSlot()
    def updateSubscriptions(self) -> None:
        newSubscriptions: set[int] = set()
        for pw in self.plotWindows.values():
            newSubscriptions.update(pw.idx2trace.keys())

        for chanidx in self.subscribedChannels.difference(newSubscriptions):
            self.zmqsubscriber.unsubscribe(chanidx)
        for chanidx in newSubscriptions.difference(self.subscribedChannels):
            self.zmqsubscriber.subscribe(chanidx)
        self.subscribedChannels = newSubscriptions

    def set_channel_groups(self, cgs: list[ChannelGroup]) -> None:
        self.channel_groups = cgs
        self.channel_index = {}
        self.channel_number = {}
        i = 0
        for cg in cgs:
            for j in range(cg.nChan):
                self.channel_index[j + cg.firstChan] = i
                self.channel_number[i] = j + cg.firstChan
                i += 1

    @pyqtSlot()
    def closeEvent(self, event: QtCore.QEvent | None) -> None:
        """Cleanly close the zmqlistener and block certain signals in the
        trigger config widget."""
        # self.triggerTab._closing()
        self.zmqsubscriber.running = False
        self.zmqthread.quit()
        self.zmqthread.wait()
        if event is not None:
            event.accept()


def version_message() -> str:
    return f"This is microscope version {__version__}"


def read_channels_json_file(args: argparse.Namespace):
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


def parsed_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog='Microscope',
        description='A GUI for plotting microcalorimeter pulses live'
    )
    parser.add_argument("-a", "--appname", default="Microscope: microcalorimeter plots")
    parser.add_argument("-r", "--rows", type=int, default=0)
    parser.add_argument("-c", "--columns", type=int, default=0)
    parser.add_argument("-N", "--nsensors", type=int, default=0)
    parser.add_argument("-i", "--indexing", action="store_true")
    parser.add_argument("-n", "--no-error-channel", action="store_true")
    parser.add_argument("-v", "--version", action="store_true")
    parser.add_argument("host", metavar="host:port", nargs="?", default="localhost:5502",
                        help="optional TCP host:port for pulse data (default localhost:5502)")
    args = parser.parse_args()
    args.tdm = not args.no_error_channel
    print(version_message())
    if args.version:
        sys.exit(0)

    # There are 3 ways to number channels. Try these in order from top to lower precedence. We can:
    # 1. use indexing,
    # 2. set rows+cols, or
    # 3. read the config file $HOME/.dastard/channels.json to learn the channel groups
    args.channel_groups = []
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


def main() -> None:
    args = parsed_arguments()
    settings = QSettings("NIST Quantum Sensors", "Microscope")

    pg.setConfigOption('background', 'w')
    pg.setConfigOption('foreground', 'k')
    pg.setConfigOptions(antialias=True)

    app = QtWidgets.QApplication(sys.argv)
    app.setWindowIcon(QtGui.QIcon("../ui/microscope.png"))
    app.setApplicationName(args.appname)
    app.setApplicationDisplayName(args.appname)

    short_ver = __version__.split("+")[0]
    title = f"{args.appname} (version {short_ver})"

    if ":" in args.host:
        host, port = args.host.split(":")
    else:
        host, port = args.host, ""
    if len(host) == 0:
        host = "localhost"
    if len(port) == 0:
        port = 5502
    else:
        port = int(port)

    window = MainWindow(settings, title, args.tdm, args.channel_groups, host, port)

    window.show()
    retval = app.exec_()
    sys.exit(retval)


if __name__ == "__main__":
    main()
