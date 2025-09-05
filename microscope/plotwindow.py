from __future__ import annotations

# Qt5 imports
import PyQt5.uic
from PyQt5.QtCore import Qt, QEvent, pyqtSlot, pyqtSignal
from PyQt5 import QtWidgets, QtGui
import pyqtgraph as pg

# other non-user imports
import numpy as np
import os

# Microscope imports
from .channel_group import ChannelGroup
from .dastardrecord import DastardRecord
from .plottraces import PlotTrace


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
            "#00aa00",  # Dark green
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
        traceHistoryLength = self.spinBox_nAverage.value()
        self.traces = [PlotTrace(i, self.color(i), traceHistoryLength) for i in range(self.NUM_TRACES)]
        self.idx2trace: dict[int, set[int]] = {}
        self.channel_groups: list[ChannelGroup] = []
        self.channel_index: dict[str, int] = {}
        self.channel_number: dict[int, int] = {}
        self.lastRecord: DastardRecord | None = None
        self.plotLimits: list[float] = [0, 1, 0, 1]
        self.setupChannels(channel_groups)
        self.setupQuickSelect(channel_groups)
        self.setupPlot()
        self.xPhysicalCheck.toggled.connect(self.xPhysicalChanged)
        self.yPhysicalCheck.toggled.connect(self.yPhysicalChanged)
        self.subtractBaselineMenu.currentIndexChanged.connect(self.redrawAll)
        self.quickFBComboBox.currentIndexChanged.connect(self.quickChannel)
        self.quickErrComboBox.currentIndexChanged.connect(self.quickChannel)
        self.waterfallDeltaSpin.valueChanged.connect(self.redrawAll)
        self.averageTraces.toggled.connect(self.spinBox_nAverage.setEnabled)
        self.averageTraces.toggled.connect(self.redrawAll)
        self.spinBox_nAverage.valueChanged.connect(self.changeAverage)
        self.plotTypeComboBox.currentIndexChanged.connect(self.plotTypeChanged)
        self.quickChanEdit.editingFinished.connect(self.channelTextChanged)

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
        self.channelSpinners: list[QtWidgets.QSpinBox] = []
        self.checkers: list[QtWidgets.QCheckBox] = []
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
            s.setAlignment(Qt.AlignmentFlag.AlignRight)
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
        assert self.mainwindow is not None
        xphys = self.mainwindow.settings.value("lastplot/xphysical", False, type=bool)
        yphys = self.mainwindow.settings.value("lastplot/yphysical", False, type=bool)
        self.xPhysicalCheck.setChecked(xphys)
        self.yPhysicalCheck.setChecked(yphys)
        self.xPhysicalChanged()
        self.yPhysicalChanged()

        xgrid = self.mainwindow.settings.value("lastplot/xgrid", True, type=bool)
        ygrid = self.mainwindow.settings.value("lastplot/ygrid", True, type=bool)
        pw.showGrid(x=xgrid, y=ygrid)
        self.setLimits(yMin=self.YMIN, yMax=self.YMAX)
        self.crosshairVLine = pg.InfiniteLine(angle=90, movable=False, pen=pg.mkColor("#aaaaaa"))
        self.crosshairHLine = pg.InfiniteLine(angle=0, movable=False, pen=pg.mkColor("#aaaaaa"))
        pw.addItem(self.crosshairVLine, ignoreBounds=True)
        pw.addItem(self.crosshairHLine, ignoreBounds=True)
        self.plotFrame.layout().addWidget(pw)
        pw.scene().sigMouseMoved.connect(self.mouseMoved)
        pw.scene().sigMouseClicked.connect(self.mouseClicked)

    def mouseMoved(self, evt: QEvent) -> None:
        pos = evt
        p1 = self.plotWidget
        if not p1.sceneBoundingRect().contains(pos):
            return

        vb = p1.getViewBox()
        mousePoint = vb.mapSceneToView(pos)
        x = mousePoint.x()
        y = mousePoint.y()
        self.crosshairVLine.setPos(x)
        self.crosshairHLine.setPos(y)
        logx, logy = vb.getState()["logMode"]
        if logx:
            x = 10**x
        if logy:
            y = 10**y
        xlabel = p1.getAxis("bottom").labelString()
        if "Samples after" in xlabel:
            xunits = "samples"
        else:
            xunits = xlabel.split("(")[-1].split(")")[0]
            x *= {"ms": 1000, "µs": 1e6, "kHz": 1e-3}.get(xunits, 1)
        msg = f"x={x:.3f} {xunits}, y={y:.1f}"
        if self.mainwindow is not None:
            self.mainwindow.statusLabel1.setText(msg)

    def mouseDoubleClickEvent(self, evt: QtGui.QMouseEvent | None) -> None:
        """If user double clicks, start auto-ranging the plot.
        If double-click on an axis label/tick area, auto-range that axis
        If double-click on the plot area, auto-range both axes
        """
        # For now, ignore single-click events.
        if evt is None:
            return

        pos = evt.pos()
        pw = self.plotWidget
        # If you needed to know the position in graphed coordinates, use:
        # view_pos = pw.getViewBox().mapSceneToView(pos)

        whichaxis = "xy"
        x_axis_rect = pw.getAxis("bottom").sceneBoundingRect()
        y_axis_rect = pw.getAxis("left").sceneBoundingRect()
        if x_axis_rect.contains(pos):
            whichaxis = "x"
        elif y_axis_rect.contains(pos):
            whichaxis = "y"
        pw.enableAutoRange(whichaxis)

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

    @pyqtSlot()
    def channelTextChanged(self) -> None:
        self.quickFBComboBox.setCurrentIndex(0)
        self.quickErrComboBox.setCurrentIndex(0)
        ch_numbers = [self.SPECIAL_INVALID] * self.NUM_TRACES
        iserr = [False] * self.NUM_TRACES
        normterms = []
        try:
            terms = self.quickChanEdit.text().lower().split(",")
            for i, rawterm in enumerate(terms):
                term = rawterm.replace(" ", "")
                if i >= self.NUM_TRACES:
                    break
                if term.startswith("e"):
                    # remove any leading combination of ero, such as "error" or "err"
                    term = term.lstrip("ero")
                    if not self.isErrvsFB:
                        iserr[i] = True
                try:
                    t = int(term)
                    if t >= 0 and t <= self.highestChan:
                        ch_numbers[i] = t
                        prefix = "e" if iserr[i] else ""
                        normterms.append(f"{prefix}{t}")
                    else:
                        normterms.append("-")
                except ValueError:
                    normterms.append("-")
                    continue

        finally:
            for spin, checker, cnum, err in zip(self.channelSpinners, self.checkers, ch_numbers, iserr):
                spin.setValue(cnum)
                checker.setChecked(err)
                prefix = "Err " if err else "Ch "
                spin.setPrefix(prefix)
            while normterms[-1] == "-":
                normterms.pop()
            self.quickChanEdit.setText(",".join(normterms))

    @pyqtSlot(int)
    def channelChanged(self, value: int) -> None:
        sending_object = self.sender()
        assert isinstance(sending_object, QtWidgets.QSpinBox)
        sender = self.channelSpinners.index(sending_object)
        self.clearTrace(sender)
        self.channelListChanged()

    def updateQuickTypeText(self) -> None:
        items = []
        for spinbox in self.channelSpinners:
            val = spinbox.value()
            if val == self.SPECIAL_INVALID:
                items.append("-")
            elif spinbox.text().startswith("Err"):
                items.append(f"e{val}")
            else:
                items.append(f"{val}")
        self.quickChanEdit.setText(",".join(items))

    @pyqtSlot(bool)
    def errStateChanged(self, _: bool) -> None:
        sending_object = self.sender()
        assert isinstance(sending_object, QtWidgets.QCheckBox)
        sender = self.checkers.index(sending_object)
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
            if chanidx not in self.idx2trace:
                self.idx2trace[chanidx] = set()
            self.idx2trace[chanidx].add(traceIdx)
            if self.isErrvsFB:
                chanidx -= 1
                if chanidx not in self.idx2trace:
                    self.idx2trace[chanidx] = set()
                self.idx2trace[chanidx].add(traceIdx)

        self.updateSubscriptions.emit()
        self.updateQuickTypeText()

    @pyqtSlot(int)
    def plotTypeChanged(self, index: int) -> None:
        self.clearAllTraces()
        isspectrum = self.isSpectrum
        self.plotWidget.setLogMode(isspectrum, isspectrum)
        self.xPhysicalChanged()
        self.yPhysicalChanged()
        if self.isTDM:
            errvfb = (index == PlotTrace.TYPE_ERR_FB)
            self.quickErrComboBox.setDisabled(errvfb)
            for cb in self.checkers:
                cb.setDisabled(errvfb)
                if errvfb:
                    cb.setChecked(False)
            self.channelListChanged()

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
        for trace in self.traces:
            trace.plotType = index
            trace.needsFFT(isspectrum)

    @pyqtSlot(DastardRecord)
    def updateReceived(self, record: DastardRecord) -> None:
        """
        Slot to handle one data record for one channel.
        It ingests the data and feeds it to the one or more `PlotTrace` objects for the channel.
        """
        if self.pauseButton.isChecked():
            return
        if record.channelIndex not in self.idx2trace:
            return

        sbtext = self.subtractBaselineMenu.currentText()
        waterfallSpacing = self.waterfallDeltaSpin.value()
        average = self.averageTraces.isChecked()
        self.lastRecord = record

        for traceIdx in self.idx2trace[record.channelIndex]:
            trace = self.traces[traceIdx]
            # Test if we're currently plotting err vs FB and this is an error
            thisIsPartner = self.isErrvsFB and record.channelIndex % 2 == 0
            if thisIsPartner:
                trace.plotpartner(record, sbtext, waterfallSpacing, average)
            else:
                trace.plotrecord(record, self.plotWidget, sbtext, waterfallSpacing, average)
        if self.isErrvsFB:
            self.setLimits(xMin=self.YMIN, xMax=self.YMAX / 2)
        elif self.isSpectrum:
            # Can't figure out how to set zoom/pan limits that make sense in log and linear space.
            # So (for now), don't try.
            self.setLimits(xMin=0, xMax=1e7)
        else:
            N = record.nSamples
            margin = 0.03
            t0 = -record.nPresamples - N * margin
            tf = t0 + N * (1.0 + 2 * margin)
            self.setLimits(xMin=t0, xMax=tf)

    def setLimits(self, xMin: float | None = None, xMax: float | None = None,
                  yMin: float | None = None, yMax: float | None = None) -> bool:
        relimit = False
        for i, new, old in zip(range(4), (xMin, xMax, yMin, yMax), self.plotLimits):
            if new is not None and new != old:
                relimit = True
                self.plotLimits[i] = new
        if relimit:
            pw = self.plotWidget
            pw.setLimits(xMin=self.plotLimits[0], xMax=self.plotLimits[1],
                         yMin=self.plotLimits[2], yMax=self.plotLimits[3])
            pw.enableAutoRange()
            self.xPhysicalChanged()
            self.yPhysicalChanged()
        return relimit

    @property
    def isSpectrum(self) -> bool:
        return self.plotTypeComboBox.currentIndex() in {PlotTrace.TYPE_PSD, PlotTrace.TYPE_RT_PSD}

    @property
    def isPSD(self) -> bool:
        return self.plotTypeComboBox.currentIndex() == PlotTrace.TYPE_PSD

    @property
    def isErrvsFB(self) -> bool:
        return self.isTDM and (self.plotTypeComboBox.currentIndex() == PlotTrace.TYPE_ERR_FB)

    @pyqtSlot()
    def xPhysicalChanged(self) -> None:
        self.xPhysicalCheck.setEnabled(not self.isSpectrum and not self.isErrvsFB)
        scale = 1.0
        if self.isSpectrum:
            label = "Frequency"
            units = "Hz"
            self.xPhysicalCheck.setChecked(True)
        elif self.isErrvsFB:
            label = "Feedback"
            units = "arbs"
            self.xPhysicalCheck.setChecked(False)
        elif self.xPhysicalCheck.isChecked():
            label = "Time after trigger"
            units = "s"
            record = self.lastRecord
            if record is not None:
                scale = record.timebase
        else:
            label = "Samples after trigger"
            units = ""
        pw = self.plotWidget
        ax = pw.getAxis("bottom")
        ax.setLabel(label, units=units)
        ax.setScale(scale)
        pw.enableAutoRange("x")
        if self.mainwindow is not None:
            self.mainwindow.settings.setValue("lastplot/xphysical", self.xPhysicalCheck.isChecked())

    @pyqtSlot()
    def yPhysicalChanged(self) -> None:
        self.yPhysicalCheck.setEnabled(not self.isErrvsFB)
        phys = self.yPhysicalCheck.isChecked()
        if phys:
            record = self.lastRecord
            if record is None:
                return
            vperarb = record.voltsPerArb

        label = "TES signal"
        units = ""
        scale = 1.0
        if self.isSpectrum:
            if phys:
                scale = vperarb
                units = "V"
            else:
                units = "arbs"

            if self.isPSD:
                label = "Power spectral density"
                scale *= scale
                units += "^2 / Hz"
            else:
                label = "sqrt(PSD)"
                units += " / sqrt(Hz)"

        elif self.isErrvsFB:
            self.yPhysicalCheck.setChecked(False)
            label = "TES signal"

        elif phys:
            scale = vperarb
            if self.isTDM:
                units = "V"
            else:
                units = "<html>&phi;0</html>"

        ax = self.plotWidget.getAxis("left")
        ax.setScale(scale)
        ax.setLabel(label, units=units)
        if self.mainwindow is not None:
            self.mainwindow.settings.setValue("lastplot/yphysical", self.yPhysicalCheck.isChecked())

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
        trace.previousRecords.clear()

    @pyqtSlot()
    def redrawAll(self) -> None:
        sbtext = self.subtractBaselineMenu.currentText()
        iswaterfall = "Waterfall" in sbtext
        self.waterfallDeltaSpin.setEnabled(iswaterfall)
        spacing = self.waterfallDeltaSpin.value()
        average = self.averageTraces.isChecked()
        for trace in self.traces:
            trace.drawStoredRecord(sbtext, spacing, average)

    closed = pyqtSignal()

    @pyqtSlot()
    def closeEvent(self, event: QEvent | None):
        plitem = self.plotWidget.getPlotItem()
        xgrid = plitem.axes["bottom"]["item"].grid
        ygrid = plitem.axes["left"]["item"].grid
        if self.mainwindow is not None:
            self.mainwindow.settings.setValue("lastplot/xgrid", xgrid)
            self.mainwindow.settings.setValue("lastplot/ygrid", ygrid)

        self.closed.emit()
        if event is not None:
            event.accept()
