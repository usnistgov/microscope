# Qt5 imports
import PyQt5.uic
from PyQt5.QtCore import Qt, pyqtSlot
from PyQt5 import QtWidgets
import pyqtgraph as pg

# other non-user imports
import numpy as np
import os
from dataclasses import dataclass
from typing import Optional

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


@dataclass(frozen=False)
class PlotTrace:
    color: str
    curve: Optional[pg.PlotCurveItem] = None
    timeAx: Optional[timeAxis] = None
    lastRecord: Optional[DastardRecord] = None
    # nPresamples: int
    # nSamples: int
    # timebase: float
    # rawdata: np.ndarray


class PlotWindow(QtWidgets.QWidget):
    """Provide the UI inside each Plot Window."""

    NUM_TRACES = 8
    SPECIAL_INVALID = -1
    YMIN = -35e3
    YMAX = 66e4

    @staticmethod
    def color(i):
        _standardColors = (
            # For most, replace standard named colors with slightly darker versions
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
        # Last one in list will be used whenever index is either too large or negative
        try:
            return _standardColors[i]
        except IndexError:
            return _standardColors[-1]

    def __init__(self, parent, channel_groups, isTDM=False):
        QtWidgets.QWidget.__init__(self, parent)
        PyQt5.uic.loadUi(os.path.join(os.path.dirname(__file__), "ui/plotwindow.ui"), self)
        self.isTDM = isTDM
        self.traces = [PlotTrace(i, self.color(i)) for i in range(self.NUM_TRACES)]
        self.idx2trace = {}
        self.setupChannels(channel_groups)
        self.setupQuickSelect(channel_groups)
        self.setupPlot()
        self.xPhysicalMenu.currentIndexChanged.connect(self.xPhysicalChanged)
        self.subtractBaselineMenu.currentIndexChanged.connect(self.redrawAll)
        self.quickFBComboBox.currentIndexChanged.connect(self.quickChannel)
        self.quickErrComboBox.currentIndexChanged.connect(self.quickChannel)
        self.waterfallDeltaSpin.valueChanged.connect(self.redrawAll)
        self.averageTraces.clicked.connect(self.startStopAveraging)

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

        self.pens = [pg.mkPen(self.color(i), width=1) for i in range(self.NUM_TRACES)]
        layout = self.channelNameLayout
        clear_grid_layout(layout)
        self.channelSpinners = []
        self.checkers = []
        for i in range(self.NUM_TRACES):
            c = "ABCDEFGHIJKL"[i]
            label = QtWidgets.QLabel(f"Trace {c}", self)
            label.setStyleSheet(f"color: {self.color(i)};")
            layout.addWidget(label, i, 0)

            s = QtWidgets.QSpinBox(self)
            s.setRange(self.SPECIAL_INVALID, self.highestChan)
            s.setSpecialValueText("--")
            s.setValue(self.SPECIAL_INVALID)
            s.setPrefix("Ch ")
            s.setAlignment(Qt.AlignRight)
            s.setMinimumWidth(75)
            s.setToolTip(f"Choose channel # for trace {c} (type '-1' to turn off)")
            s.valueChanged.connect(self.channelChanged)
            self.channelSpinners.append(s)
            layout.addWidget(s, i, 1)

            box = QtWidgets.QCheckBox(self)
            tool = f"Trace {c} use error signal"
            box.setToolTip(tool)
            box.toggled.connect(self.errStateChanged)
            self.checkers.append(box)
            if self.isTDM:
                layout.addWidget(box, i, 2)
            else:
                box.setEnabled(False)
                box.hide()

    def setupQuickSelect(self, channel_groups):
        if not self.isTDM:
            self.quickErrLabel.hide()
            self.quickErrComboBox.hide()
            self.quickFBLabel.setText("Quick select Chan #")
        self.quickSelectIndex = [None]
        for box in (self.quickFBComboBox, self.quickErrComboBox):
            box.clear()
            box.addItem("")
        for i, cg in enumerate(channel_groups):
            n = cg.nChan
            nentries = (n-1+self.NUM_TRACES) // self.NUM_TRACES
            for j in range(nentries):
                cstart = cg.firstChan + j*self.NUM_TRACES
                cend = cstart + self.NUM_TRACES - 1
                if cend > cg.lastChan:
                    cstart -= (cend - cg.lastChan)
                    cend = cg.lastChan
                label = f"Ch {cstart}-{cend} (group {i})"
                self.quickFBComboBox.addItem(label)
                self.quickSelectIndex.append((cstart, cend))
                if self.isTDM:
                    elabel = f"Err {cstart}-{cend} (group {i})"
                    self.quickErrComboBox.addItem(elabel)

    def setupPlot(self):
        p1 = pg.PlotWidget()
        p1.setWindowTitle("LJH pulse record")
        self.plotFrame.layout().addWidget(p1)
        p1.setLabel("left", "TES current")
        p1.setLabel("bottom", "Samples after trigger")
        # p1.addLegend()
        self.plotWidget = p1
        p1.setLimits(yMin=self.YMIN, yMax=self.YMAX)

    @pyqtSlot(bool)
    def pausePressed(self, bool): pass
    @pyqtSlot()
    def savePlot(self): pass
    @pyqtSlot()
    def startStopAveraging(self): pass

    @pyqtSlot(int)
    def quickChannel(self, index):
        iserror = self.isTDM and self.sender() == self.quickErrComboBox
        prefix = "Err " if iserror else "Ch "
        if index < 1:
            return
        cstart, cend = self.quickSelectIndex[index]
        for c in range(cstart, cend+1):
            i = c - cstart
            self.channelSpinners[i].setValue(c)
            self.channelSpinners[i].setPrefix(prefix)
            self.checkers[i].setChecked(iserror)

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
        prefix = "Err " if self.checkers[sender].isChecked() else "Ch "
        self.channelSpinners[sender].setPrefix(prefix)
        self.channelListChanged()

    def channelListChanged(self):
        self.idx2trace = {}
        for traceIdx, spinner in enumerate(self.channelSpinners):
            channum = spinner.value()
            if self.isTDM:
                chanidx = 2*channum + 1
                if self.checkers[traceIdx].isChecked():
                    chanidx -= 1
            else:
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

    @property
    def xPhysical(self):
        return "ms" in self.xPhysicalMenu.currentText()

    def plotrecord(self, traceIdx, record):
        curve = self.traces[traceIdx].curve
        if curve is None:
            xaxis = timeAxis.create(record.nPresamples, record.nSamples, record.timebase)
            self.traces[traceIdx].timeAx = xaxis
            x = xaxis.x(self.xPhysical)
            curve = self.plotWidget.plot(x, record.record, pen=self.pens[traceIdx])
            self.traces[traceIdx].curve = curve
        else:
            xaxis = self.traces[traceIdx].timeAx
            if (xaxis.nPresamples != record.nPresamples or xaxis.nSamples != record.nSamples or
                    xaxis.timebase != record.timebase):
                xaxis = timeAxis.create(record.nPresamples, record.nSamples, record.timebase)
                self.traces[traceIdx].timeAx = xaxis
        self.traces[traceIdx].lastRecord = record
        self.draw(traceIdx)

    @pyqtSlot()
    def xPhysicalChanged(self):
        # Do something to choose x-axis ranges: current value AND max range
        pw = self.plotWidget
        x_range, _ = pw.viewRange()
        timeAxes = [t.timeAx for t in self.traces]
        plot_is_empty = timeAxes.count(None) == self.NUM_TRACES
        if plot_is_empty:
            timebase = 0.01
        else:
            timebase = np.mean([ax.timebase for ax in timeAxes if ax is not None])

        if self.xPhysical:
            x_range[0] *= timebase*1000
            x_range[1] *= timebase*1000
            pw.setLabel("bottom", "Time after trigger", units="ms")
        else:
            x_range[0] /= timebase*1000
            x_range[1] /= timebase*1000
            pw.setLabel("bottom", "Samples after trigger", units="")
        if not plot_is_empty:
            xmin = np.min([ax.x(self.xPhysical)[0] for ax in timeAxes if ax is not None])
            xmax = np.max([ax.x(self.xPhysical)[-1] for ax in timeAxes if ax is not None])
            pw.setLimits(xMin=xmin, xMax=xmax, yMin=self.YMIN, yMax=self.YMAX)
        pw.setXRange(x_range[0], x_range[1])
        self.redrawAll()

    @pyqtSlot()
    def clearGraphs(self):
        for traceIdx in range(self.NUM_TRACES):
            self.clearTrace(traceIdx)

    def clearTrace(self, traceIdx):
        curve = self.traces[traceIdx].curve
        if curve is None:
            return
        self.plotWidget.removeItem(curve)
        self.traces[traceIdx].curve = None
        self.traces[traceIdx].lastRecord = None

    @pyqtSlot()
    def redrawAll(self):
        for traceIdx in range(len(self.traces)):
            self.draw(traceIdx)

    def draw(self, traceIdx):
        curve = self.traces[traceIdx].curve
        if curve is None:
            return

        xaxis = self.traces[traceIdx].timeAx
        x = xaxis.x(self.xPhysical)
        sbtext = self.subtractBaselineMenu.currentText()
        if "Raw" in sbtext:
            ydata = self.traces[traceIdx].lastRecord.record

        elif "Subtract" in sbtext:
            ydata = self.traces[traceIdx].lastRecord.record_baseline_subtracted

        if "Waterfall" in sbtext:
            spacing = self.waterfallDeltaSpin.value()
            ydata = self.traces[traceIdx].lastRecord.record_baseline_subtracted + traceIdx * spacing
            self.waterfallDeltaSpin.setEnabled(True)
        else:
            self.waterfallDeltaSpin.setEnabled(False)

        self.traces[traceIdx].curve.setData(x, ydata)
