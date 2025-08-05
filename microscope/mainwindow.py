import importlib.resources

# Qt5 imports
import PyQt5.uic
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import QSettings, pyqtSlot
import pyqtgraph as pg

# User code imports
from . import plotwindow
from . import subscriber
from .dialogs import AboutDialog, HelpDialog
from .channel_group import ChannelGroup


def find_resource(filename):
    package_name = "microscope"
    return str(importlib.resources.files(package_name).joinpath(filename))


class MainWindow(QtWidgets.QMainWindow):  # noqa: PLR0904
    def __init__(self, settings: QSettings, title: str, isTDM: bool, channel_groups: list[ChannelGroup],
                 host: str, port: int) -> None:
        self.settings = settings
        self.isTDM = isTDM
        self.channel_groups: list[ChannelGroup] = []
        self.channel_index: dict[int, int] = {}
        self.channel_number: dict[int, int] = {}
        self.set_channel_groups(channel_groups)

        pg.setConfigOption('background', 'w')
        pg.setConfigOption('foreground', 'k')
        pg.setConfigOptions(antialias=True)

        parent = None
        QtWidgets.QMainWindow.__init__(self, parent)
        icon = QtGui.QIcon(find_resource("ui/microscope.png"))
        self.setWindowIcon(icon)
        PyQt5.uic.loadUi(find_resource("ui/microscope.ui"), self)
        self.setWindowTitle(title)
        self.title = title
        self.actionAbout.triggered.connect(self.showAbout)
        self.actionPlot_tips.triggered.connect(self.showPlotTips)

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
        self.zmqthread.started.connect(self.zmqsubscriber.data_monitor_loop)
        QtCore.QTimer.singleShot(0, self.zmqthread.start)

        self.plotWindows: dict[tuple[int, int], plotwindow.PlotWindow] = {}
        self.addPlotWindow()
        self.addPlotsButton.clicked.connect(self.addPlotWindow)

    @pyqtSlot()
    def showAbout(self) -> None:
        dialog = AboutDialog(self)
        dialog.show()

    @pyqtSlot()
    def showPlotTips(self) -> None:
        dialog = HelpDialog(self)
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
            # print(f"Unsubscribing  idx {chanidx}")
            self.zmqsubscriber.unsubscribe(chanidx)
        for chanidx in newSubscriptions.difference(self.subscribedChannels):
            # print(f"Subscribing to idx {chanidx}")
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

        windows = [v for v in self.plotWindows.values() if v is not None]
        for pw in windows:
            pw.close()
        if event is not None:
            event.accept()
