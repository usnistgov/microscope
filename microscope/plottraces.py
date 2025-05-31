from __future__ import annotations

# Qt5 imports
from PyQt5.QtCore import pyqtSlot
import pyqtgraph as pg

# other non-user imports
import numpy as np
from dataclasses import dataclass
from collections import deque

# local imports
from .dastardrecord import DastardRecord, meanDastardRecord


@dataclass(frozen=True)
class timeAxis:
    nPresamples: int
    nSamples: int
    timebase_sec: float
    x: np.ndarray

    @staticmethod
    def create(nPresamples: int, nSamples: int, timebase_sec: int | float) -> timeAxis:
        xsamples = np.arange(nSamples) - nPresamples
        return timeAxis(nPresamples, nSamples, timebase_sec, xsamples)


class PlotTrace:
    TYPE_TIMESERIES = 0
    TYPE_PSD = 1
    TYPE_RT_PSD = 2
    TYPE_ERR_FB = 3

    DEFAULT_REDRAW_TIME = 0.25  # seconds

    def __init__(self, idx: int, color: str, traceHistoryLength: int) -> None:
        self.traceIdx = idx
        self.color = color
        self.pen = pg.mkPen(color, width=1)
        self.curve: pg.PlotDataItem | None = None
        self.timeAx: timeAxis | None = None
        self.previousRecords: deque[DastardRecord] = deque([], traceHistoryLength)
        self.previousPartners: deque[DastardRecord] = deque([], traceHistoryLength)
        self.previousPSD: deque[np.ndarray] = deque([], traceHistoryLength)
        self.computingFFT = False
        self.plotType = self.TYPE_TIMESERIES
        self.redrawTime_ns = 1e9 * self.DEFAULT_REDRAW_TIME
        self.lastdrawTime_ns = np.uint64(0)

    @pyqtSlot(int)
    def changeBufferLength(self, cap: int) -> None:
        self.previousRecords = deque(self.previousRecords, cap)
        self.previousPartners = deque(self.previousPartners, cap)
        self.previousPSD = deque(self.previousPSD, cap)

    def needsFFT(self, FFT: bool) -> None:
        self.computingFFT = FFT
        if FFT and len(self.previousPSD) == 0:
            for record in self.previousRecords:
                PSD = record.PSD()
                self.previousPSD.append(PSD)
        if not FFT:
            self.previousPSD.clear()

    def saverecord(self, record: DastardRecord, isPartner: bool = False) -> None:
        # If the record is not compatible with previous, clear the queue.
        if len(self.previousRecords) > 0:
            if record.incompatible(self.previousRecords[-1]):
                self.previousRecords.clear()
                self.previousPartners.clear()
                self.previousPSD.clear()

        if isPartner:
            self.previousPartners.append(record)
            return

        self.previousRecords.append(record)
        if self.computingFFT:
            PSD = record.PSD()
            self.previousPSD.append(PSD)

    def plotpartner(self, partner: DastardRecord, sbtext: str,
                    waterfallSpacing: int | float, average: bool) -> None:
        self.saverecord(partner, isPartner=True)
        self.drawStoredRecord(sbtext, waterfallSpacing, average)

    def plotrecord(self, record: DastardRecord, plotWidget: pg.PlotWidget,
                   sbtext: str, waterfallSpacing: int | float, average: bool) -> None:
        self.saverecord(record)
        if self.curve is None or self.timeAx is None:
            xaxis = timeAxis.create(record.nPresamples, record.nSamples, record.timebase)
            self.timeAx = xaxis
            self.curve = plotWidget.plot(xaxis.x, record.record, pen=self.pen)
        else:
            xaxis = self.timeAx
            if (xaxis.nPresamples != record.nPresamples or xaxis.nSamples != record.nSamples or
                    xaxis.timebase_sec != record.timebase):
                xaxis = timeAxis.create(record.nPresamples, record.nSamples, record.timebase)
                self.timeAx = xaxis
        self.drawStoredRecord(sbtext, waterfallSpacing, average)

    def drawStoredRecord(self, sbtext: str, waterfallSpacing: int | float, average: bool) -> None:
        xaxis = self.timeAx
        if self.curve is None or xaxis is None or len(self.previousRecords) == 0:
            return

        # Don't draw this record if one was drawn too recently
        # Currently, "too recent" is a fixed time. In the future, this could be user-selectable.
        record = self.previousRecords[-1]
        dt_ns = record.triggerTime_ns - self.lastdrawTime_ns
        if dt_ns < self.redrawTime_ns:
            return
        self.lastdrawTime_ns = record.triggerTime_ns

        if self.plotType == self.TYPE_TIMESERIES:
            if average:
                record = meanDastardRecord(self.previousRecords)
            if "Raw" in sbtext:
                ydata = np.asarray(record.record, dtype=float)

            elif "Subtract" in sbtext:
                ydata = record.record_baseline_subtracted

            elif "Waterfall" in sbtext:
                ydata = record.record_baseline_subtracted + self.traceIdx * waterfallSpacing
            x = xaxis.x

        elif self.isSpectrum:
            if average:
                ydata = meanPSD(self.previousPSD)
            else:
                ydata = self.previousPSD[-1]
            if self.plotType == self.TYPE_RT_PSD:
                ydata = np.sqrt(ydata)
            if "Waterfall" in sbtext:
                ydata *= waterfallSpacing ** self.traceIdx
            x = record.FFTFreq()

        elif self.plotType == self.TYPE_ERR_FB:
            if len(self.previousPartners) == 0:
                return
            partner = self.previousPartners[-1]
            if partner.incompatible(record) or (partner.frameIndex != record.frameIndex):
                return
            if average:
                record = meanDastardRecord(self.previousRecords)
                partner = meanDastardRecord(self.previousPartners)
            x = np.asarray(record.record, dtype=float)
            ydata = np.asarray(partner.record, dtype=float)

        self.curve.setData(x, ydata)

    @property
    def isSpectrum(self) -> bool:
        return self.plotType in {self.TYPE_PSD, self.TYPE_RT_PSD}


def meanPSD(psdbuffer: deque[np.ndarray]) -> np.ndarray:
    n = len(psdbuffer)
    assert n > 0

    raw = np.zeros_like(psdbuffer[0], dtype=float)
    for dr in psdbuffer:
        raw += dr
    return raw / n
