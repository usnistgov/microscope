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


class PulseHistory:
    def __init__(self, capacity: int, shift: int | None = None, dtype: type = np.float32):
        self.cap = capacity
        self.used = 0
        if shift is None:
            shift = max(self.cap // 10, 10)
        self.shift = shift
        self.data = np.zeros(self.cap, dtype=dtype)

    def _truncate(self, min_to_remove: int = 1):
        # Always remove at least min_to_remove, AND always at least self.shift
        nremove = max(min_to_remove, self.shift)
        print(f"Truncating! Removing {nremove} values")
        self.data[:self.used - nremove] = self.data[nremove:self.used]
        self.used -= nremove

    def append(self, value):
        if self.used >= self.cap:
            self._truncate()
        self.data[self.used] = value
        self.used += 1

    def extend(self, values):
        n = len(values)
        if n + self.used >= self.cap:
            self._truncate(n)
        self.data[self.used:self.used + n] = values[:]
        self.used += n

    def clear(self):
        self.used = 0

    @property
    def series(self):
        return self.data[:self.used]


class PlotTrace:
    TYPE_PULSERECORD = 0
    TYPE_PSD = 1
    TYPE_RT_PSD = 2
    TYPE_ERR_FB = 3
    TYPE_TIMESERIES = 4

    DEFAULT_REDRAW_TIME = 0.25  # seconds
    HISTORY_LENGTH = 8192  # enough for 1 hour at ~2.2 cps

    def __init__(self, idx: int, color: str, traceHistoryLength: int) -> None:
        self.traceIdx = idx
        self.color = color
        self.pen = pg.mkPen(color, width=1)  # for lines
        self.brush = pg.mkBrush(color)  # for symbols

        self.curve: pg.PlotDataItem | None = None
        self.timeAx: timeAxis | None = None
        self.previousRecords: deque[DastardRecord] = deque([], traceHistoryLength)
        self.previousPartners: deque[DastardRecord] = deque([], traceHistoryLength)
        self.previousPSD: deque[np.ndarray] = deque([], traceHistoryLength)
        self.summary_pulse_rms = PulseHistory(self.HISTORY_LENGTH, dtype=np.float32)
        self.summary_pulse_avg = PulseHistory(self.HISTORY_LENGTH, dtype=np.float32)
        self.summary_pulse_peak = PulseHistory(self.HISTORY_LENGTH, dtype=np.float32)
        self.summary_pretrig_mean = PulseHistory(self.HISTORY_LENGTH, dtype=np.float32)
        self.summary_time_ns = PulseHistory(self.HISTORY_LENGTH, dtype=np.uint64)
        self.computingFFT = False
        self.plotType = self.TYPE_PULSERECORD
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

    def clearhistory(self, clearSummaries: bool = False) -> None:
        self.previousRecords.clear()
        self.previousPartners.clear()
        self.previousPSD.clear()
        if clearSummaries:
            self.summary_pulse_rms.clear()
            self.summary_pulse_avg.clear()
            self.summary_pulse_peak.clear()
            self.summary_pretrig_mean.clear()
            self.summary_time_ns.clear()

    def saverecord(self, record: DastardRecord, isPartner: bool = False) -> None:
        # If the record is not compatible with previous, clear the queue.
        if len(self.previousRecords) > 0:
            if record.incompatible(self.previousRecords[-1]):
                self.clearhistory()

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
                   sbtext: str, tstext: str, waterfallSpacing: int | float, average: bool) -> None:
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
        self.drawStoredRecord(sbtext, tstext, waterfallSpacing, average)

    def drawStoredRecord(self, sbtext: str, tstext: str, waterfallSpacing: int | float, average: bool) -> None:
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

        # Default plot arguments: lines, no symbol. (Override this for timeseries plots.)
        plotargs = {
            "symbol": None,
        }

        if self.plotType == self.TYPE_PULSERECORD:
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

        elif self.plotType == self.TYPE_TIMESERIES:
            x = self.summary_time_ns.series
            x = (x[:] - x[0]) / 1e9
            ydata = self.summary_named(tstext).series
            plotargs["symbol"] = "o"
            plotargs["symbolPen"] = self.pen
            plotargs["symbolSize"] = 5
            plotargs["symbolBrush"] = self.brush

        self.curve.setData(x, ydata, **plotargs)

    @property
    def isSpectrum(self) -> bool:
        return self.plotType in {self.TYPE_PSD, self.TYPE_RT_PSD}

    def summary_named(self, tstext: str) -> PulseHistory:
        tstext = tstext.lower()
        if "peak" in tstext:
            return self.summary_pulse_peak
        elif "average" in tstext:
            return self.summary_pulse_avg
        elif "pretrigger" in tstext:
            return self.summary_pretrig_mean
        return self.summary_pulse_rms


def meanPSD(psdbuffer: deque[np.ndarray]) -> np.ndarray:
    n = len(psdbuffer)
    assert n > 0

    raw = np.zeros_like(psdbuffer[0], dtype=float)
    for dr in psdbuffer:
        raw += dr
    return raw / n
