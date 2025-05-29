from __future__ import annotations

# Qt5 imports
import PyQt5.uic
from PyQt5.QtCore import Qt, QEvent, pyqtSlot, pyqtSignal
from PyQt5 import QtWidgets
import pyqtgraph as pg

# other non-user imports
import numpy as np
import os
from dataclasses import dataclass

from dastardrecord import DastardRecord, ListBasedBuffer, DastardRecordsBuffer
from channel_group import ChannelGroup


def clear_grid_layout(grid_layout: QtWidgets.QGridLayout) -> None:
    while grid_layout.count():
        item = grid_layout.itemAt(0)
        if item is None:
            continue
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
    timebase_sec: float
    xsamples: np.ndarray
    xseconds: np.ndarray

    def x(self, isphysical: bool) -> np.ndarray:
        return self.xseconds if isphysical else self.xsamples

    @staticmethod
    def create(nPresamples: int, nSamples: int, timebase_sec: int | float) -> timeAxis:
        xsamples = np.arange(nSamples) - nPresamples
        xseconds = xsamples * timebase_sec
        return timeAxis(nPresamples, nSamples, timebase_sec, xsamples, xseconds)


class PlotTrace:
    TYPE_TIMESERIES = 0
    TYPE_PSD = 1
    TYPE_RT_PSD = 2
    TYPE_ERR_FB = 3

    def __init__(self, idx: int, color: str, previousBufferLength: int) -> None:
        self.traceIdx = idx
        self.color = color
        self.pen = pg.mkPen(color, width=1)
        self.curve: pg.PlotDataItem | None = None
        self.timeAx: timeAxis | None = None
        self.lastRecord: DastardRecord | None = None
        self.previousRecords = DastardRecordsBuffer(previousBufferLength)
        self.previousPSD: ListBasedBuffer[np.ndarray] = ListBasedBuffer(previousBufferLength)
        self.computingFFT = False
        self.plotType = self.TYPE_TIMESERIES

    @pyqtSlot(int)
    def changeBufferLength(self, cap: int) -> None:
        self.previousRecords.resize(cap)
        self.previousPSD.resize(cap)

    def needsFFT(self, FFT: bool) -> None:
        self.computingFFT = FFT
        if FFT and len(self.previousPSD) == 0:
            for record in self.previousRecords.buffer:
                PSD = record.PSD()
                self.previousPSD.push(PSD)
        if not FFT:
            self.previousPSD.clear()

    def plotrecord(self, record: DastardRecord, plotWidget: pg.PlotWidget, xPhysical: bool,
                   sbtext: str, waterfallSpacing: int | float, average: bool) -> None:
        if self.curve is None or self.timeAx is None:
            xaxis = timeAxis.create(record.nPresamples, record.nSamples, record.timebase)
            self.timeAx = xaxis
            x = xaxis.x(xPhysical)
            self.curve = plotWidget.plot(x, record.record, pen=self.pen)
        else:
            xaxis = self.timeAx
            if (xaxis.nPresamples != record.nPresamples or xaxis.nSamples != record.nSamples or
                    xaxis.timebase_sec != record.timebase):
                xaxis = timeAxis.create(record.nPresamples, record.nSamples, record.timebase)
                self.timeAx = xaxis
        self.lastRecord = record
        self.previousRecords.push(record)
        if self.computingFFT:
            PSD = record.PSD()
            self.previousPSD.push(PSD)
        self.drawStoredRecord(xPhysical, sbtext, waterfallSpacing, average)

    def drawStoredRecord(self, xPhysical: bool, sbtext: str,
                         waterfallSpacing: int | float, average: bool) -> None:
        xaxis = self.timeAx
        record = self.lastRecord
        if self.curve is None or xaxis is None or record is None:
            return

        x = xaxis.x(xPhysical)
        if self.plotType == self.TYPE_TIMESERIES:
            if average:
                record = self.previousRecords.mean()
            if "Raw" in sbtext:
                ydata = np.asarray(record.record, dtype=float)

            elif "Subtract" in sbtext:
                ydata = record.record_baseline_subtracted

            elif "Waterfall" in sbtext:
                ydata = record.record_baseline_subtracted + self.traceIdx * waterfallSpacing

        if self.isSpectrum:
            if average:
                ydata = meanPSD(self.previousPSD)
            else:
                ydata = self.previousPSD.last()
            if self.plotType == self.TYPE_RT_PSD:
                ydata = np.sqrt(ydata)
            if "Waterfall" in sbtext:
                ydata *= waterfallSpacing ** self.traceIdx
            x = record.FFTFreq()
        self.curve.setData(x, ydata)

    @property
    def isSpectrum(self) -> bool:
        return self.plotType in {self.TYPE_PSD, self.TYPE_RT_PSD}


def meanPSD(psdbuffer: ListBasedBuffer[np.ndarray]) -> np.ndarray:
    n = len(psdbuffer)
    assert n > 0

    if n == 1:
        return psdbuffer.buffer[0]

    raw = psdbuffer.buffer[0].copy()
    for dr in psdbuffer.buffer[1:]:
        raw += dr
    return raw / n


class PlotWindow(QtWidgets.QWidget):  # noqa: PLR0904
    """Provide the UI inside each Plot Window."""

    NUM_TRACES = 8
    SPECIAL_INVALID = -1
    YMIN = -35e3
    YMAX = 66e3

    @staticmethod
    def color(i: int) -> str:
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

    def __init__(self, parent: QtWidgets.QWidget | None,
                 channel_groups, isTDM=False) -> None:
        QtWidgets.QWidget.__init__(self, parent)
        self.mainwindow = parent
        PyQt5.uic.loadUi(os.path.join(os.path.dirname(__file__), "ui/plotwindow.ui"), self)
        self.isTDM = isTDM
        previousBufferLength = self.spinBox_nAverage.value()
        self.traces = [PlotTrace(i, self.color(i), previousBufferLength) for i in range(self.NUM_TRACES)]
        self.idx2trace: dict[int, set[int]] = {}
        self.channel_groups: list[ChannelGroup] = []
        self.channel_index: dict[str, int] = {}
        self.channel_number: dict[int, int] = {}
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
        self.plotTypeComboBox.currentIndexChanged.connect(self.plotTypeChanged)

    @pyqtSlot(int)
    def changeAverage(self, n: int) -> None:
        for trace in self.traces:
            trace.changeBufferLength(n)

    def setupChannels(self, channel_groups: list[ChannelGroup]) -> None:
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

    def setupQuickSelect(self, channel_groups: list[ChannelGroup]) -> None:
        if not self.isTDM:
            self.quickErrLabel.hide()
            self.quickErrComboBox.hide()
            self.quickFBLabel.setText("Quick select Chan #")
        self.quickSelectIndex: list[tuple[int, int]] = [(-1, -1)]
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

    def setupPlot(self) -> None:
        if not self.isTDM:
            self.plotTypeComboBox.model().item(3).setEnabled(False)
        pw = pg.PlotWidget()
        self.plotWidget = pw
        pw.setWindowTitle("LJH pulse record")
        pw.setLabel("left", "TES current")
        self.setupXAxis()
        pw.setLimits(yMin=self.YMIN, yMax=self.YMAX)
        self.crosshairVLine = pg.InfiniteLine(angle=90, movable=False, pen=pg.mkColor("#aaaaaa"))
        self.crosshairHLine = pg.InfiniteLine(angle=0, movable=False, pen=pg.mkColor("#aaaaaa"))
        pw.addItem(self.crosshairVLine, ignoreBounds=True)
        pw.addItem(self.crosshairHLine, ignoreBounds=True)
        self.plotFrame.layout().addWidget(pw)
        pw.scene().sigMouseMoved.connect(self.mouseMoved)

    def mouseMoved(self, evt) -> None:
        pos = evt
        p1 = self.plotWidget
        vb = p1.getViewBox()
        if p1.sceneBoundingRect().contains(pos):
            mousePoint = vb.mapSceneToView(pos)
            x = mousePoint.x()
            y = mousePoint.y()
            self.crosshairVLine.setPos(x)
            self.crosshairHLine.setPos(y)
            xlabel = p1.getAxis("bottom").labelString()
            if "Samples after" in xlabel:
                xunits = "samples"
            else:
                xunits = xlabel.split("(")[-1].split(")")[0]
                x *= {"ms": 1000, "µs": 1e6, "kHz": 1e-3}.get(xunits, 1)
            msg = f"x={x:.3f} {xunits}, y={y:.1f}"
            self.mainwindow.statusLabel1.setText(msg)

    def setupXAxis(self, enableAutoRange: bool = True) -> None:
        pw = self.plotWidget
        if self.isSpectrum:
            pw.setLabel("bottom", "Frequency", units="Hz")
            self.xPhysicalMenu.setCurrentIndex(1)
            self.xPhysicalMenu.model().item(0).setEnabled(False)
            pw.setLimits(xMin=1, xMax=1e6)
        else:
            self.xPhysicalMenu.model().item(0).setEnabled(True)
            if self.xPhysical:
                pw.setLabel("bottom", "Time after trigger", units="s")
            else:
                pw.setLabel("bottom", "Samples after trigger")
            pw.setLimits(xMin=-1e5, xMax=1e5)
        if enableAutoRange:
            pw.autoRange()
            pw.enableAutoRange()

    @pyqtSlot(bool)
    def pausePressed(self, paused: bool) -> None: pass

    @pyqtSlot(int)
    def quickChannel(self, index: int) -> None:
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
    def channelChanged(self, value: int) -> None:
        sender = self.channelSpinners.index(self.sender())
        self.clearTrace(sender)
        self.channelListChanged()

    @pyqtSlot(bool)
    def errStateChanged(self, _: bool) -> None:
        sender = self.checkers.index(self.sender())
        self.clearTrace(sender)
        prefix = "Err " if self.checkers[sender].isChecked() else "Ch "
        self.channelSpinners[sender].setPrefix(prefix)
        self.channelListChanged()

    updateSubscriptions = pyqtSignal()

    def channelListChanged(self) -> None:
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
        self.updateSubscriptions.emit()

    @pyqtSlot(int)
    def plotTypeChanged(self, index: int) -> None:
        self.clearAllTraces()
        self.setupXAxis()
        if self.isTDM:
            errvfb = (index == PlotTrace.TYPE_ERR_FB)
            self.quickErrComboBox.setDisabled(errvfb)
            for cb in self.checkers:
                cb.setDisabled(errvfb)
                if errvfb:
                    cb.setChecked(False)
        isspectrum = index in {PlotTrace.TYPE_PSD, PlotTrace.TYPE_RT_PSD}
        self.subtractBaselineMenu.model().item(1).setEnabled(not isspectrum)
        if isspectrum:
            self.subtractBaselineMenu.setCurrentIndex(0)
            self.waterfallLabel.setText("Waterfall r.")
            self.waterfallDeltaSpin.setToolTip("Ratio between traces in waterfall mode")
            self.waterfallDeltaSpin.setValue(2.0)
        else:
            self.waterfallLabel.setText("<html><p>Waterfall &Delta;</p></html>")
            self.waterfallDeltaSpin.setToolTip("Vertical spacing between traces in waterfall mode")
            self.waterfallDeltaSpin.setValue(1000.0)
        self.plotWidget.setLogMode(isspectrum, isspectrum)
        for trace in self.traces:
            trace.plotType = index
            trace.needsFFT(isspectrum)

    @pyqtSlot(DastardRecord)
    def updateReceived(self, record: DastardRecord) -> None:
        """
        Slot to handle one data record for one channel.
        It ingests the data and feeds it to the BaselineFinder object for the channel.
        """
        if self.pauseButton.isChecked():
            return
        sbtext = self.subtractBaselineMenu.currentText()
        waterfallSpacing = self.waterfallDeltaSpin.value()
        average = self.averageTraces.isChecked()
        plotWasEmpty = self.plot_is_empty

        if record.channelIndex in self.idx2trace:
            for traceIdx in self.idx2trace[record.channelIndex]:
                trace = self.traces[traceIdx]
                trace.plotrecord(record, self.plotWidget, self.xPhysical, sbtext, waterfallSpacing, average)
        if plotWasEmpty:
            pw = self.plotWidget
            pw.autoRange()
            ((xmin, xmax), _) = pw.viewRange()
            pw.setLimits(xMin=xmin, xMax=xmax)

    @property
    def isSpectrum(self) -> bool:
        return self.plotTypeComboBox.currentIndex() in {PlotTrace.TYPE_PSD, PlotTrace.TYPE_RT_PSD}

    @property
    def xPhysical(self) -> bool:
        return "physical" in self.xPhysicalMenu.currentText()

    @property
    def plot_is_empty(self) -> bool:
        return [t.timeAx for t in self.traces].count(None) == self.NUM_TRACES

    @pyqtSlot()
    def xPhysicalChanged(self) -> None:
        # Do something to choose x-axis ranges: current value AND max range
        self.setupXAxis(enableAutoRange=False)
        if not self.plot_is_empty:
            timeAxes = [t.timeAx for t in self.traces]
            timebase_sec = np.mean([ax.timebase_sec for ax in timeAxes if ax is not None])
            pw = self.plotWidget
            if self.xPhysical:
                rescale = timebase_sec
            else:
                rescale = 1 / timebase_sec
            (xrangemin, xrangemax), _ = pw.viewRange()
            (xlimitmin, xlimitmax) = pw.getViewBox().getState()["limits"]["xLimits"]
            xlimitmin *= rescale
            xlimitmax *= rescale
            xrangemin *= rescale
            xrangemax *= rescale
            pw.setLimits(xMin=xlimitmin, xMax=xlimitmax)
            pw.setXRange(xrangemin, xrangemax)
        self.redrawAll()

    @pyqtSlot()
    def clearAllTraces(self) -> None:
        for traceIdx in range(self.NUM_TRACES):
            self.clearTrace(traceIdx)

    def clearTrace(self, traceIdx: int) -> None:
        trace = self.traces[traceIdx]
        curve = trace.curve
        if curve is None:
            return
        self.plotWidget.removeItem(curve)
        trace.curve = None
        trace.lastRecord = None
        trace.previousRecords.clear()

    @pyqtSlot()
    def redrawAll(self) -> None:
        sbtext = self.subtractBaselineMenu.currentText()
        iswaterfall = "Waterfall" in sbtext
        self.waterfallDeltaSpin.setEnabled(iswaterfall)
        spacing = self.waterfallDeltaSpin.value()
        average = self.averageTraces.isChecked()
        for trace in self.traces:
            trace.drawStoredRecord(self.xPhysical, sbtext, spacing, average)

    closed = pyqtSignal()

    @pyqtSlot()
    def closeEvent(self, event: QEvent | None):
        self.closed.emit()
        if event is not None:
            event.accept()
