from __future__ import annotations
import numpy as np
import struct
from collections import deque
from dataclasses import dataclass, replace
from functools import cache

__info__ = """
Some notes on estimating power spectral density (PSD) using the discrete Fourier Transform (DFT).

Because we are calculating the power spectral density, we should do some
windowing to prevent excessive leakage. basically, multiply each number by some
scaling factor that is ~0 at the edges and ~unity in the middle. we do this b/c
we see a snapshot of the entire data stream and there is considerable spectral
leakage from one bin to the next. because at the left and right, we have only
the tailing or leading bins to look at, we do not get a complete picture of
where they came from. so, instead, we hardly look at those points, where the
information is incomplete and look more at the middle points where we know both
trailing and leading edges. windowing methods accomplish this. see numerical
recipes p553

Note about the normalization for delta t fftw gives us volts per root
sample. We want volts per root hz, so we need to have a conversion factor
that does this. Given Randy's code, it seems this factor should be root
(2*deltaT).

Note from Randy D, found in xcaldaqclient, then MATTER, then the C++ microscope:
Be sure that you are averaging the SQUARED signals (PSD in V2/Hz) and then displaying
the sqrt of that average.  If you, instead, just average the V/sqrt(Hz) signal,
you will be off by an insidiously small gain factor, something like 8%,
depending on the f binning!  This was a hard-won bug removal for me.
"""

# Memoize (`@cache`) these two functions (DFT window and frequency bin values) because we expect
# to use with the same arguments over and over.


@cache
def windowFunction(N: int) -> np.ndarray:
    """windowFunction(N)

    Parameters
    ----------
    N : int
        Length of the data being DFTed

    Returns
    -------
    np.ndarray
        An appropriate window function, normalized.

    When multiplying data by a window function, the amount of power in the signal can be
    changed.  To compensate for this, the mean(window[i]^2) is scaled to 1.
    """
    window = np.hanning(N)
    window *= 1 / np.sqrt((window**2).mean())
    return window


@cache
def FFTFreq(N: int, dt: int | float) -> np.ndarray:
    return np.fft.rfftfreq(N, d=dt)


@dataclass(frozen=True)
class DastardRecord:
    channelIndex: int
    datatype: type
    nPresamples: int
    nSamples: int
    timebase: float
    voltsPerArb: float
    triggerTime: np.uint64
    frameIndex: np.uint16
    record: np.ndarray

    header_fmt = "<HBBIIffQQ"
    data_fmt = ("b", "B", "<h", "<H", "<i", "<I", "<q", "<Q")
    data_type = (np.int8, np.uint8, np.int16, np.uint16, np.int32, np.uint32, np.int64, np.uint64)

    @property
    def record_baseline_subtracted(self) -> np.ndarray:
        return self.record - self.record[:self.nPresamples - 1].mean()

    @classmethod
    def fromBinary(cls, header: bytes, contents: bytes) -> DastardRecord:
        values = struct.unpack(cls.header_fmt, header)
        chanidx, version, typecode, nPresamples, nSamples, timebase, voltsPerArb, triggerTime, frameIndex = values
        assert version == 0
        data_fmt = cls.data_fmt[typecode]
        datatype = cls.data_type[typecode]
        data = np.frombuffer(contents, dtype=data_fmt)

        return DastardRecord(chanidx, datatype, nPresamples, nSamples, timebase, voltsPerArb,
                             triggerTime, frameIndex, data)

    def PSD(self) -> np.ndarray:
        window = windowFunction(self.nSamples)
        fft = np.fft.rfft(window * (self.record - self.record.mean()))
        return np.abs(fft**2).real

    def FFTFreq(self) -> np.ndarray:
        return FFTFreq(self.nSamples, self.timebase)

    def incompatible(self, other: DastardRecord, rtol: float = 1e-2) -> bool:
        """incompatible - Does this DastardRecord have records incompatible with `other`?

        Parameters
        ----------
        other : DastardRecord
            An earlier record for comparison

        rtol : float
            The relative tolerance on sample rates.

        Returns
        -------
        bool
            Whether the `other` is incompatible with this record. Will be True if
            the number of samples or of pretrigger samples are unequal, or if the
            sample time differs by more than relative tolerance `rtol`.

        """
        if (self.nPresamples != other.nPresamples) or \
                (self.nSamples != other.nSamples):
            return True
        return abs(other.timebase / self.timebase - 1) > rtol


def meanDastardRecord(buffer: deque[DastardRecord]) -> DastardRecord:
    """meanDastardRecord - Compute a DastardRecord equal to the mean of the records in `buffer`.

    Parameters
    ----------
    buffer : deque[DastardRecord]
        A history of the last N DastardRecords on this channel.

    Returns
    -------
    DastardRecord
        A synthetic DastardRecord whose pulse payload equals the mean of those of all the
        records in the deque. The mean record will have floating-point values.
    """
    n = len(buffer)
    assert n > 0

    if n == 1:
        return buffer[0]

    raw = np.zeros_like(buffer[0].record, dtype=float)
    for dr in buffer:
        raw += dr.record
    return replace(buffer[-1], record=raw / n)
