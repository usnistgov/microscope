# Qt5 imports
import PyQt5.uic
from PyQt5.QtCore import Qt, pyqtSlot
from PyQt5 import QtWidgets
import pyqtgraph as pg

# other non-user imports
import numpy as np
import os
from dataclasses import dataclass

from dastardrecord import DastardRecord


def clear_grid_layout(grid_layout):
    while grid_layout.count():
        item = grid_layout.itemAt(0)
        widget = item.widget()
        if widget is not None:
            grid_layout.removeWidget(widget)
            widget.deleteLater()
        else:
            grid_layout.removeItem(item)


@dataclass(frozen=True)
class timeAxis:
    nPresamples: int
    nSamples: int
    timebase: float
    xsamples: np.ndarray
    xms: np.ndarray

    def x(self, isphysical):
        return self.xms if isphysical else self.xsamples

    @staticmethod
    def create(nPresamples, nSamples, timebase):
        xsamples = np.arange(nSamples) - nPresamples
        xms = xsamples * timebase * 1000
        return timeAxis(nPresamples, nSamples, timebase, xsamples, xms)


class PlotWindow(QtWidgets.QWidget):
    """Provide the UI inside each Plot Window."""

    NUM_TRACES = 8
    SPECIAL_INVALID = -1

    standardColors = (
        # For most, use QColor to replace standard named colors with slightly darker versions
        "black",
        "#b400e6",  # Purple
        "#0000b4",  # Blue
        "#00bebe",  # Cyan
        "darkgreen",
        "#cdcd00",  # Gold
        "#ff8000",  # Orange
        "red",
        "gray"
    )

    def __init__(self, parent, channel_groups, isTDM=False):
        QtWidgets.QWidget.__init__(self, parent)
        PyQt5.uic.loadUi(os.path.join(os.path.dirname(__file__), "ui/plotwindow.ui"), self)
        self.isTDM = isTDM
        self.timeAxes = [None for i in range(self.NUM_TRACES)]
        self.curves = [None for i in range(self.NUM_TRACES)]
        self.lastRecord = [None for i in range(self.NUM_TRACES)]
        self.idx2trace = {}
        self.setupChannels(channel_groups)
        self.setupPlot()
        self.xPhysicalCheck.stateChanged.connect(self.xPhysicalChanged)
        self.subtractBaselineCheck.stateChanged.connect(self.redrawAll)

    def setupChannels(self, channel_groups):
        self.channel_groups = channel_groups
        self.channel_index = {}
        self.channel_number = {}
        i = 0
        for cg in channel_groups:
            for j in range(cg.nChan):
                self.channel_index[j+cg.firstChan] = i
                self.channel_number[i] = j+cg.firstChan
                i += 1
        self.highestChan = np.max([cg.lastChan for cg in channel_groups])

        self.pens = [pg.mkPen(c, width=1) for c in self.standardColors]
        layout = self.channelNameLayout
        clear_grid_layout(layout)
        self.channelSpinners = []
        self.checkers = []
        for i in range(self.NUM_TRACES):
            c = "ABCDEFGHIJKL"[i]
            label = QtWidgets.QLabel(f"Trace {c}", self)
            label.setStyleSheet(f"color: {self.standardColors[i]};")
            layout.addWidget(label, i, 0)

            s = QtWidgets.QSpinBox(self)
            s.setRange(self.SPECIAL_INVALID, self.highestChan)
            s.setSpecialValueText("--")
            s.setValue(self.SPECIAL_INVALID)
            s.setPrefix("Ch ")
            s.setAlignment(Qt.AlignRight)
            s.setMinimumWidth(75)
            s.valueChanged.connect(self.channelChanged)
            self.channelSpinners.append(s)
            layout.addWidget(s, i, 1)

            if self.isTDM:
                box = QtWidgets.QCheckBox(self)
                tool = f"Trace {c} use error signal"
                box.setToolTip(tool)
                box.toggled.connect(self.errStateChanged)
                self.checkers.append(box)
                layout.addWidget(box, i, 2)

    def setupPlot(self):
        p1 = pg.PlotWidget()
        p1.setWindowTitle("LJH pulse record")
        self.plotFrame.layout().addWidget(p1)
        p1.setLabel("left", "TES current")
        p1.setLabel("bottom", "Samples after trigger")
        # p1.addLegend()
        self.plotWidget = p1

    @pyqtSlot(bool)
    def pausePressed(self, bool): pass
    @pyqtSlot()
    def savePlot(self): pass

    @pyqtSlot(int)
    def channelChanged(self, value):
        sender = self.channelSpinners.index(self.sender())
        print(f"Next chan: {value} sent by spinner #{sender}")
        self.clearTrace(sender)
        self.channelListChanged()

    @pyqtSlot(bool)
    def errStateChanged(self, value):
        sender = self.checkers.index(self.sender())
        print(f"Err state: {value} sent by checkbox #{sender}")
        self.clearTrace(sender)
        self.channelListChanged()

    def channelListChanged(self):
        self.idx2trace = {}
        for traceIdx, spinner in enumerate(self.channelSpinners):
            channum = spinner.value()
            if channum not in self.channel_index:
                continue
            chanidx = self.channel_index[channum]
            if chanidx in self.idx2trace:
                self.idx2trace[chanidx].add(traceIdx)
            else:
                self.idx2trace[chanidx] = {traceIdx}

    @pyqtSlot(DastardRecord)
    def updateReceived(self, record):
        """
        Slot to handle one data record for one channel.
        It ingests the data and feeds it to the BaselineFinder object for the channel.
        """
        if self.pauseButton.isChecked():
            return
        if record.channelIndex in self.idx2trace:
            for traceIdx in self.idx2trace[record.channelIndex]:
                self.plotrecord(traceIdx, record)

    def plotrecord(self, traceIdx, record):
        curve = self.curves[traceIdx]
        if curve is None:
            xaxis = timeAxis.create(record.nPresamples, record.nSamples, record.timebase)
            self.timeAxes[traceIdx] = xaxis
            x = xaxis.x(self.xPhysicalCheck.isChecked())
            curve = self.plotWidget.plot(x, record.record, pen=self.pens[traceIdx])
            self.curves[traceIdx] = curve
        else:
            xaxis = self.timeAxes[traceIdx]
            if (xaxis.nPresamples != record.nPresamples or xaxis.nSamples != record.nSamples or
                    xaxis.timebase != record.timebase):
                xaxis = timeAxis.create(record.nPresamples, record.nSamples, record.timebase)
                self.timeAxes[traceIdx] = xaxis
        self.lastRecord[traceIdx] = record
        self.draw(traceIdx)

    @pyqtSlot()
    def xPhysicalChanged(self):
        # Do something to choose x-axis ranges: current value AND max range
        physical = self.xPhysicalCheck.isChecked()
        pw = self.plotWidget
        x_range, _ = pw.viewRange()
        timebase = np.mean([ax.timebase for ax in self.timeAxes if ax is not None])
        if physical:
            x_range[0] *= timebase*1000
            x_range[1] *= timebase*1000
            pw.setLabel("bottom", "Time after trigger", units="ms")
        else:
            x_range[0] /= timebase*1000
            x_range[1] /= timebase*1000
            pw.setLabel("bottom", "Samples after trigger", units="")
        xmin = np.min([ax.x(physical)[0] for ax in self.timeAxes if ax is not None])
        xmax = np.max([ax.x(physical)[-1] for ax in self.timeAxes if ax is not None])
        vb = pw.getViewBox()
        vb.setLimits(xMin=xmin, xMax=xmax)
        vb.setXRange(x_range[0], x_range[1])
        self.redrawAll()

    @pyqtSlot()
    def clearGraphs(self):
        for traceIdx in range(self.NUM_TRACES):
            self.clearTrace(traceIdx)

    def clearTrace(self, traceIdx):
        curve = self.curves[traceIdx]
        if curve is None:
            return
        self.plotWidget.removeItem(curve)
        self.curves[traceIdx] = None
        self.lastRecord[traceIdx] = None

    @pyqtSlot()
    def redrawAll(self):
        for (traceIdx, curve) in enumerate(self.curves):
            self.draw(traceIdx)

    def draw(self, traceIdx):
        curve = self.curves[traceIdx]
        if curve is None:
            return

        xaxis = self.timeAxes[traceIdx]
        x = xaxis.x(self.xPhysicalCheck.isChecked())
        ydata = self.lastRecord[traceIdx].record
        if self.subtractBaselineCheck.isChecked():
            ydata = self.lastRecord[traceIdx].record_baseline_subtracted
        self.curves[traceIdx].setData(x, ydata)
