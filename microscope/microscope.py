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
from PyQt5 import QtGui, QtWidgets
from PyQt5.QtCore import QSettings, QCoreApplication

# Non-Qt imports
import argparse
import json
import pathlib
import sys

# User code imports
from channel_group import ChannelGroup
from mainwindow import MainWindow, find_resource

try:
    from _version import version as __version__
    from _version import version_tuple
except ImportError:
    __version__ = "unknown version"
    version_tuple = (0, 0, "unknown version")


QCoreApplication.setOrganizationName("NIST Quantum Sensors")
QCoreApplication.setOrganizationDomain("nist.gov")
QCoreApplication.setApplicationName("Microscope")


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

    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName(args.appname)
    app.setApplicationDisplayName(args.appname)
    icon = QtGui.QIcon(QtGui.QPixmap(find_resource("ui/microscope.png")))
    app.setWindowIcon(icon)

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
