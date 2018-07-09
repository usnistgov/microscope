#include <iostream>
#include <math.h>
#include "fftcomputer.h"

/*

Some notes on the discrete Fourier transform and FFTW's implementation:

We are ignoring the advice of Numerical Recipes p505 (2nd ed, or p610 3rd ed):
"in fact, we categorically recommend that you _only_ use FFTs with N a power of
two. If the length of your data set is not a power of two pad it with zeros up
to the next power of two." But at some point, Randy requested to replace
padding with not padding. ?? Plus, fftw has its whole "plan" phase, so
arbitrary N shouldn't be SO bad.

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

If you put in N real numbers into a DFT, you get out N/2+1 complex numbers
The FFTW convention "Half Complex" is a way of packing these into a
length-N vector exactly.

Let's say we have N complex numbers where the kth number is composed of real
imaginary parts R(k) + I(k) instead of having an array of complex numbers with
length n, we construct an array of length n in the following order: R[0], R[1],
R[2], ...,R[N/2], I[ ((N+1)/2)-1 ], ..., I[2], I[1] Because of symmetry for
even N, the 0th elements and the Nyquist element have no imaginary part and are
therefore omitted in the second half of the array. If N is odd, the Nyquist
frequency lies between indicies and the highest I[] is included. The result for
either even or odd N: the input and output arrays are the same length.

*/

///
/// \brief FFTComputer::FFTComputer
///
FFTComputer::FFTComputer() :
    fftIn(NULL),
    fftOut(NULL),
    window(NULL),
    plan_made(false)
{
}


///
/// \brief FFTComputer::prepare
/// \param length_in
///
void FFTComputer::prepare(int length_in) {
    length = length_in;
    std::cout << "Planning DFTs for data of length " << length << std::endl;
    nfreq = 1 + (length/2);

    // allocate space for the various arrays
    window = new double[length];
    fftIn = (double*) fftw_malloc(sizeof(double) * length);
    fftOut = (double*) fftw_malloc(sizeof(double) * length);

    // create a new plan (a function from the fftw3 library)
    plan = fftw_plan_r2r_1d(
        length,        // the length of the plan
        fftIn,         // the "in" array
        fftOut,        // the "out" array
        FFTW_R2HC,     // the "direction" of the transform, in this case Real to Half Complex
        FFTW_MEASURE); // how "optimal" you want the FFT routines
        // FFTW_MEASURE uses more inital overhead to achieve less per-calc cost.
    plan_made = true;

//    // Recompute the x (frequency) axis for plots of this FFT.
//    // Careful! We are going to drop the zero-frequency bin.
//    const double frequencyStep = m_sampleRate / m_fftLength;
//    for (unsigned int i = 0; i<m_fftNumFrequencies-1; i++) {
//        m_plotDataX[i] = double(i+1) * frequencyStep;
//    }

    // Compute a Hann window for (possibly) apodizing the data
    // This is the numpy, not the NumRec version. That is, the window
    // is strictly zero at the included endpoints: i=0 and i=(length-1).
    const double scalingFactor = 2. * M_PI / (length-1);
    double sumsq = 0.0;
    for (int i=0; i<length; i++) {
        window[i] = 0.5*(1 - cos( scalingFactor*i ));
        sumsq += window[i]*window[i];
    }

    // When using a window function, the amount of power in the signal is reduced.
    // To compensate for this, the average(window[i]^2) should be scaled to 1.
    const double meansq = sumsq / length;
    for (int i=0; i<length; i++) {
        window[i] /= meansq;
    }
}


///
/// \brief FFTComputer::computePSD
/// \param data
/// \param psd
/// \param sampleRate
/// \param useWindow
/// \param mean
///
void FFTComputer::computePSD(QVector<double> &data, QVector<double> &psd, double sampleRate,
                             bool useWindow, double &mean) {
    // Copy the data from the pulse to the input array and accumulate the data sum.
    // subtract the PREV average from each value and multiply by the apodize value
    // use prev to do this so that the DC bin isn't crazy, and leakage from
    // it to neighbor is controlled. (Note that it's still enough to make the
    // lowest-f bin dominate, but by less than before.)
    double avgSamp = 0;
    if (useWindow) {
        for (int i=0; i<length; i++) {
            fftIn[i] = window[i]*(data[i] - mean);
            avgSamp += data[i];
        }
    } else {
        for (int i=0; i<length; i++) {
            fftIn[i] = data[i] - mean;
            avgSamp += data[i];
        }
    }

    // The average level for this record is returned in mean; save it for next record
    mean = avgSamp / length;

    // execute the plan (actually calculate the fft)
    fftw_execute(plan);

    /*
    Note about normalization in fft found this in the fftw tutorial:
    Users should note that FFTW computes an unnormalized DFT.
    Thus, computing a forward followed by a backward transform
    (or vice versa) results in the original array scaled by n.
    it never says it, but i guess the normalizing factor is therefore
    1/sqrt(n) for magnitude and 1/n for PSD (which we're computing).
    */
    const double fftwNormalizingFactor = 1.0/double(length);

    /*
     Note about the normalization for delta t fftw gives us volts per root
     sample. We want volts per root hz, so we need to have a conversion factor
     that does this. Given Randy's code, it seems this factor should be root
     (2*deltaT).
     */
    const double deltaTNormalizingFactor = 2./sampleRate;
    const double normalizingFactor = fftwNormalizingFactor * deltaTNormalizingFactor;

    // now take the output array and use it to construct an magnitude array that is normalized
    /*
     Loop through the out buffer and generate the magnitude from the half
     complex (out) vector (see fftw's manual on half complex arrays)

     Note from Randy:
     Be sure that you are averaging the SQUARED signals (V2/Hz) and then displaying
     the sqrt of that average.  If you, instead, just average the V/sqrt(Hz) signal,
     you will be off by an insidiously small gain factor, something like 8%,
     depending on the f binning!  This was a hard-won bug removal in some of my
     code ~3 years ago.

     Take our normalized v/root(hz) function and square it (v^2/hz).  Use the power spectrum to
     compute the average power. when it comes time to plot it (v/root(hz)) take the sqrt
     of the average.
    */

    psd.resize(nfreq);

    psd[0] = (fftOut[0]*fftOut[0])*normalizingFactor;
    for (int i=1; i<nfreq; i++) {
        double thisPSDbin  = fftOut[i]*fftOut[i] + fftOut[length-i]*fftOut[length-i];
        psd[i] = thisPSDbin * normalizingFactor;
    }
    // Correct for double-counting the Nyquist bin, if N is even.
    if (length%2 == 0)
        psd[length/2] *= 0.5;
}


///
/// \brief FFTComputer::~FFTComputer
///
FFTComputer::~FFTComputer() {
    delete []window;
    fftw_free(fftIn);
    fftw_free(fftOut);
    if (plan_made)
        fftw_destroy_plan(plan);
    plan_made = false;
}





///
/// \brief FFTMaster::FFTMaster
///
FFTMaster::FFTMaster()
{
    computers.reserve(10);
}


///
/// \brief FFTMaster::computePSD
/// \param data
/// \param psd
/// \param mean
///
void FFTMaster::computePSD(QVector<double> &data, QVector<double> &psd, double sampleRate,
                             bool useWindow, double &mean) {
    int length = data.size();
    if (!computers.contains(length)) {
        computers[length].prepare(length);
    }
    FFTComputer *computer = &computers[length];
//    // TODO: here remove old preparations, once there are too many.
//    // Will need to keep track of the order they were added, so delete the oldest.

    computer->computePSD(data, psd, sampleRate, useWindow, mean);
    std::cout << "computePSD returned; last value: " << psd[psd.size()-1] << std::endl;
}

