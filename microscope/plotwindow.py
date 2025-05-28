# Qt5 imports
import PyQt5.uic
from PyQt5.QtCore import Qt, pyqtSlot, pyqtSignal, QObject
from PyQt5 import QtWidgets
import pyqtgraph as pg

# other non-user imports
import numpy as np
import os
from dataclasses import dataclass

from dastardrecord import DastardRecord, DastardRecordsBuffer


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


class PlotTrace(QObject):
    def __init__(self, idx, color, previousBufferLength):
        self.traceIdx = idx
        self.color = color
        self.pen = pg.mkPen(color, width=1)
        self.curve = None
        self.timeAx = None
        self.lastRecord = None
        self.previousRecords = DastardRecordsBuffer(previousBufferLength)

    @pyqtSlot(int)
    def changeBufferLength(self, cap):
        self.previousRecords.resize(cap)

    def plotrecord(self, record, plotWidget, xPhysical, sbtext, waterfallSpacing, average):
        if self.curve is None:
            xaxis = timeAxis.create(record.nPresamples, record.nSamples, record.timebase)
            self.timeAx = xaxis
            x = xaxis.x(xPhysical)
            self.curve = plotWidget.plot(x, record.record, pen=self.pen)
        else:
            xaxis = self.timeAx
            if (xaxis.nPresamples != record.nPresamples or xaxis.nSamples != record.nSamples or
                    xaxis.timebase != record.timebase):
                xaxis = timeAxis.create(record.nPresamples, record.nSamples, record.timebase)
                self.timeAx = xaxis
        self.lastRecord = record
        self.previousRecords.push(record)
        self.drawStoredRecord(xPhysical, sbtext, waterfallSpacing, average)

    def drawStoredRecord(self, xPhysical, sbtext, waterfallSpacing, average):
        if self.curve is None:
            return

        xaxis = self.timeAx
        x = xaxis.x(xPhysical)
        record = self.lastRecord
        if average:
            record = self.previousRecords.mean()
        if "Raw" in sbtext:
            ydata = np.asarray(record.record, dtype=float)

        elif "Subtract" in sbtext:
            ydata = record.record_baseline_subtracted

        elif "Waterfall" in sbtext:
            ydata = record.record_baseline_subtracted + self.traceIdx * waterfallSpacing

        self.curve.setData(x, ydata)


class PlotWindow(QtWidgets.QWidget):
    """Provide the UI inside each Plot Window."""

    NUM_TRACES = 8
    SPECIAL_INVALID = -1
    YMIN = -35e3
    YMAX = 66e3

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
        previousBufferLength = self.spinBox_nAverage.value()
        self.traces = [PlotTrace(i, self.color(i), previousBufferLength) for i in range(self.NUM_TRACES)]
        self.idx2trace = {}
        self.setupChannels(channel_groups)
        self.setupQuickSelect(channel_groups)
        self.setupPlot()
        self.xPhysicalMenu.currentIndexChanged.connect(self.xPhysicalChanged)
        self.subtractBaselineMenu.currentIndexChanged.connect(self.redrawAll)
        self.quickFBComboBox.currentIndexChanged.connect(self.quickChannel)
        self.quickErrComboBox.currentIndexChanged.connect(self.quickChannel)
        self.waterfallDeltaSpin.valueChanged.connect(self.redrawAll)
        self.averageTraces.toggled.connect(self.spinBox_nAverage.setEnabled)
        self.averageTraces.toggled.connect(self.redrawAll)
        self.spinBox_nAverage.valueChanged.connect(self.changeAverage)

    @pyqtSlot(int)
    def changeAverage(self, n):
        for trace in self.traces:
            trace.changeBufferLength(n)

    def setupChannels(self, channel_groups):
        self.channel_groups = channel_groups
        self.channel_index = {}
        self.channel_name = {}
        i = 0
        for cg in channel_groups:
            for j in range(cg.nChan):
                cnum = cg.firstChan + j
                if self.isTDM:
                    name = f"Err {cnum}"
                    self.channel_index[name] = i
                    self.channel_name[i] = name
                    i += 1
                name = f"Ch {cnum}"
                self.channel_index[name] = i
                self.channel_name[i] = name
                i += 1
        self.highestChan = np.max([cg.lastChan for cg in channel_groups])

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
            nentries = (n - 1 + self.NUM_TRACES) // self.NUM_TRACES
            for j in range(nentries):
                cstart = cg.firstChan + j * self.NUM_TRACES
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
    def pausePressed(self, paused): pass
    @pyqtSlot()
    def savePlot(self): pass

    @pyqtSlot(int)
    def quickChannel(self, index):
        # Do not act if the empty menu item is chosen.
        if index < 1:
            return

        iserror = self.isTDM and self.sender() == self.quickErrComboBox
        if iserror:
            prefix = "Err "
            self.quickFBComboBox.setCurrentIndex(0)
        else:
            prefix = "Ch "
            self.quickErrComboBox.setCurrentIndex(0)
        cstart, cend = self.quickSelectIndex[index]
        for c in range(cstart, cend + 1):
            i = c - cstart
            self.channelSpinners[i].setValue(c)
            self.channelSpinners[i].setPrefix(prefix)
            self.checkers[i].setChecked(iserror)

    @pyqtSlot(int)
    def channelChanged(self, value):
        sender = self.channelSpinners.index(self.sender())
        self.clearTrace(sender)
        self.channelListChanged()

    @pyqtSlot(bool)
    def errStateChanged(self, value):
        sender = self.checkers.index(self.sender())
        self.clearTrace(sender)
        prefix = "Err " if self.checkers[sender].isChecked() else "Ch "
        self.channelSpinners[sender].setPrefix(prefix)
        self.channelListChanged()

    def channelListChanged(self):
        self.idx2trace = {}
        for traceIdx, spinner in enumerate(self.channelSpinners):
            channel_name = spinner.text()
            if channel_name not in self.channel_index:
                continue
            chanidx = self.channel_index[channel_name]
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
        sbtext = self.subtractBaselineMenu.currentText()
        waterfallSpacing = self.waterfallDeltaSpin.value()
        average = self.averageTraces.isChecked()

        if record.channelIndex in self.idx2trace:
            for traceIdx in self.idx2trace[record.channelIndex]:
                trace = self.traces[traceIdx]
                trace.plotrecord(record, self.plotWidget, self.xPhysical, sbtext, waterfallSpacing, average)

    @property
    def xPhysical(self):
        return "ms" in self.xPhysicalMenu.currentText()

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
            x_range[0] *= timebase * 1000
            x_range[1] *= timebase * 1000
            pw.setLabel("bottom", "Time after trigger", units="ms")
        else:
            x_range[0] /= timebase * 1000
            x_range[1] /= timebase * 1000
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
        trace = self.traces[traceIdx]
        curve = trace.curve
        if curve is None:
            return
        self.plotWidget.removeItem(curve)
        trace.curve = None
        trace.lastRecord = None
        trace.previousRecords.clear()

    @pyqtSlot()
    def redrawAll(self):
        sbtext = self.subtractBaselineMenu.currentText()
        iswaterfall = "Waterfall" in sbtext
        self.waterfallDeltaSpin.setEnabled(iswaterfall)
        spacing = self.waterfallDeltaSpin.value()
        average = self.averageTraces.isChecked()
        for trace in self.traces:
            trace.drawStoredRecord(self.xPhysical, sbtext, spacing, average)

    closed = pyqtSignal()

    @pyqtSlot()
    def closeEvent(self, event):
        self.closed.emit()
        event.accept()
