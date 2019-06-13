#include "refreshplots.h"

///////////////////////////////////////////////////////////////////////////////
/// \file refreshplots.cpp
/// Defines refreshPlots class, to perform work at regular timed intervals
/// in its own thread (by subclassing periodicUpdater). Specifically, it updates
/// the plotted data curves on a single plotWindow object.
///


#include <iostream>
#include <QVector>
#include <math.h>

#include "fftcomputer.h"
#include "microscope.h"
#include "periodicupdater.h"
#include "pulsehistory.h"
#include "refreshplots.h"



////////////////////////////////////////////////////////////////////////////////////
/// \brief Constructor
/// \param client_in    The active Client object
/// \param msec_period  The refresh time in milliseconds
///
refreshPlots::refreshPlots(int msec_period) :
    periodicUpdater(msec_period),
    ms_per_sample(1),
    last_freq_step(0.0),
    plottingPaused(false),
    ErrVsFeedback(false),
    isPSD(false),
    isFFT(false),
//    isHistogram(false),
    averaging(false),
    nAverage(16), // matches the initial value of spinBox_nAverage in QtDesigner
    analysisType(ANALYSIS_PULSE_RMS)
{
    // Fill the list of channels to be plotted with the no-plot indicator.
    const int INITIAL_TRACES=8;
    const int DONT_PLOT = -1;
    channels.resize(INITIAL_TRACES);
    for (int i=0; i<INITIAL_TRACES; i++)
        channels[i] = DONT_PLOT;
    lastSerial.resize(INITIAL_TRACES);
    for (int i=0; i<INITIAL_TRACES; i++)
        lastSerial[i] = -1;

    // Let plots have time-zero reference of now, rounded down to next exact hour
    gettimeofday(&time_zero, nullptr);
    time_zero.tv_usec = 0;
    time_zero.tv_sec -= time_zero.tv_sec%3600;

//    scratch.resize(INITIAL_TRACES);
//    histograms.clear();
//    for (int i=0; i<INITIAL_TRACES; i++) {
//        histograms.push_back(new Histogram);
//    }

    fftMaster = new FFTMaster;

    const int PULSES_TO_STORE_AKA_MAX_AVERAGES=128;
    // needs to match:
    // RESERVE in pulsehistory.cpp line 34, not totally sure
    // spinBox_nAverage maximum value, set in QtDesigner as of May 2019
    pulseHistories.reserve(INITIAL_TRACES);
    for (int i=0; i<INITIAL_TRACES; i++) {
        pulseHistory *h = new pulseHistory(PULSES_TO_STORE_AKA_MAX_AVERAGES, fftMaster);
        pulseHistories.append(h);
    }
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Destructor: deletes the Histogram objects.
///
refreshPlots::~refreshPlots()
{
//    for (unsigned int i=0; i<histograms.size(); i++)
//        delete histograms[i];
    for (int i=0; i<pulseHistories.size(); i++)
        delete pulseHistories[i];
}


////////////////////////////////////////////////////////////////////////////////////
/// \brief receiveNewData
/// \param channum
/// \param data
/// \param length
///
void refreshPlots::receiveNewData(int tracenum, pulseRecord *pr) {
    if (tracenum < 0 || tracenum >= pulseHistories.size())
        return;
    struct timeval now;
    gettimeofday(&now, nullptr);
    pr->dtime = now.tv_sec-time_zero.tv_sec + now.tv_usec*1e-6;
    pulseHistories[tracenum]->insertRecord(pr);
}


///////////////////////////////////////////////////////////////////////////////
/// \brief Slot to be informed that the sample time has changed.
/// \param dt  The new sample time (in seconds)
///
void refreshPlots::newSampleTime(double dt)
{
    if (approx_equal(dt*1000., ms_per_sample, 1e-5))
        return;
    ms_per_sample = dt*1000.;
    frequencies.clear();
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief refreshPlots::toggledAveraging
/// \param doAvg
///
void refreshPlots::toggledAveraging(bool doAvg) {
    if (averaging == doAvg)
        return;
    averaging = doAvg;

    // Make every plot "expire" by marking it as old.
    for (int trace=0; trace<channels.size(); trace++)
        lastSerial[trace] = -1;
}


///////////////////////////////////////////////////////////////////////////////
/// \brief Slot when nAverage changes
/// \param nAverage   The new number of traces to average
void refreshPlots::nAverageChanged(int new_nAverage)
{
    nAverage = new_nAverage;
    return;
}


////////////////////////////////////////////////////////////////////////////////////
/// \brief refreshPlots::toggledDFTing
/// \param dft
///
void refreshPlots::toggledDFTing(bool dft) {
    if (doingDFT == dft)
        return;
    doingDFT = dft;

    // Make every plot "expire" by marking it as old.
    for (int trace=0; trace<channels.size(); trace++) {
        lastSerial[trace] = -1;
        pulseHistories[trace]->setDoDFT(dft);
    }
}


////////////////////////////////////////////////////////////////////////////////////
/// \brief The run loop. All repeated work must appear here.
///
void refreshPlots::workQuantum(void) {
    if (plottingPaused)
        return;

    if (isPSD || isFFT)
        refreshSpectrumPlots();
    else if (isTimeseries)
        refreshTimeseriesPlots();
//    else if (isHistogram)
//        refreshHistograms();
    else
        refreshStandardPlots();
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Clear all stored data for building histograms, including the "scratch"
/// vectors of raw data that we use before hist limits are fixed.
///
void refreshPlots::clearStoredData()
{
    for (int trace=0; trace<channels.size(); trace++) {
        lastSerial[trace] = pulseHistories[trace]->uses();
        pulseHistories[trace]->clearAllData();
    }
    //    for (unsigned int i=0; i<histograms.size(); i++) {
    //        histograms[i]->clear();
    //        scratch[i].clear();
    //    }
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Called by the run loop once to draw standard (non-spectrum) plots
///
void refreshPlots::refreshStandardPlots()
{
    for (int trace=0; trace<channels.size(); trace++) {
        int channum = channels[trace];
        if (channum < 0)
            continue;

        // Check: have we already plotted this record? If so, don't replot.
        if (pulseHistories[trace]->uses() <= lastSerial[trace])
            continue;
        lastSerial[trace] = pulseHistories[trace]->uses();

        if (averaging) {
            pulseRecord *mean = pulseHistories[trace]->meanRecord(nAverage);
            if (mean != nullptr)
                emit newDataToPlot(trace, mean->data, mean->presamples, mean->voltsperarb*1000.);
            delete mean;
        } else {
            pulseRecord *record = pulseHistories[trace]->newestRecord();
            if (record != nullptr)
                emit newDataToPlot(trace, record->data, record->presamples, record->voltsperarb*1000.);
        }
    }
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Called by the run loop once to draw standard (non-spectrum) plots
///
void refreshPlots::refreshSpectrumPlots()
{
    for (int trace=0; trace<channels.size(); trace++) {
        int channum = channels[trace];
        if (channum < 0)
            continue;

        // Check: have we already plotted this record? If so, don't replot.
        if (pulseHistories[trace]->uses() <= lastSerial[trace])
            continue;
        lastSerial[trace] = pulseHistories[trace]->uses();

        const QVector<double> *psdData;
        if (averaging) {
            psdData = pulseHistories[trace]->meanPSD(nAverage);
        } else {
            psdData = pulseHistories[trace]->newestPSD();
        }
        if (psdData == nullptr)
            continue;

        const int nfreq = psdData->size();
        const double freq_step = 1e3/(ms_per_sample * pulseHistories[trace]->samples());
        if (nfreq != frequencies.size() || !approx_equal(freq_step, last_freq_step, 1e-5)) {
            frequencies.resize(nfreq);
            for (int i=0; i<nfreq; i++)
                frequencies[i] = i * freq_step;
            last_freq_step = freq_step;
        }

        double mVPerArb = 1.0;
        pulseRecord *pr = pulseHistories[trace]->newestRecord();
        if (pr != nullptr)
            mVPerArb = 1000 * double(pr->voltsperarb);

        if (isPSD) {
            emit newDataToPlot(trace, frequencies, *psdData, 1.0, mVPerArb*mVPerArb);
        } else {
            QVector<double> fft(nfreq);
            for (int i=0; i<nfreq; i++) {
                fft[i] = sqrt((*psdData)[i]);
            }
            emit newDataToPlot(trace, frequencies, fft, 1.0, mVPerArb);
        }
    }
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Called by the run loop once to draw standard (non-spectrum) plots
///
void refreshPlots::refreshTimeseriesPlots()
{
    for (int trace=0; trace<channels.size(); trace++) {
        int channum = channels[trace];
        if (channum < 0)
            continue;

        // Check: have we already plotted this record? If so, don't replot.
        const int numNew = pulseHistories[trace]->uses() - lastSerial[trace];
        if (numNew <= 0)
            continue;
        lastSerial[trace] = pulseHistories[trace]->uses();

        QVector<double> times = pulseHistories[trace]->times();
        QVector<double> ydata;
        switch (analysisType) {
        case ANALYSIS_BASELINE:
            ydata = pulseHistories[trace]->baseline();
            break;
        case ANALYSIS_PULSE_MEAN:
            ydata = pulseHistories[trace]->mean();
            break;
        case ANALYSIS_PULSE_MAX:
            ydata = pulseHistories[trace]->peak();
            break;
        case ANALYSIS_PULSE_RMS:
        default:
            ydata = pulseHistories[trace]->rms();
            break;
        }

        int offset = times.size() - numNew;
        QVector<double> x = times.mid(offset);
        QVector<double> y = ydata.mid(offset);
        emit addDataToPlot(trace, x, y);
    }
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Called by the run loop once to draw histograms
///
//void refreshPlots::refreshHistograms()
//{
//    for (unsigned int trace=0; trace<channels.size(); trace++) {
//        int channum = channels[trace];
//        if (channum < 0)
//            continue;

////        xdaq::StreamChannel *schan = client->streamData[channum];
////        double oldest_time = double(lastTimes[trace])/1e6;
//        std::vector<double> timevec, valuevec;
////        schan->getRecentAnalysis(timevec, valuevec, oldest_time, analysisType);
//        const size_t n = timevec.size();
//        if (n==0)
//            continue;
////        lastTimes[trace] = counter_t(timevec.back()*1e6);

//        // Now handle the data. We'll choose from 2 routes, depending on whether the
//        // existing data in the histogram is enough to consider its limits fixed.
//        const size_t NDATA_TO_FIX_HISTS = 100;
//        Histogram *hist = histograms[trace];
//        const size_t n_earlier = hist->entries();
//        if (n_earlier >= NDATA_TO_FIX_HISTS) {
//            hist->update(valuevec);
//        } else {

//            // Okay, we are still exploring the data to set the hist limits.
//            std::vector<double> *scr = &scratch[trace];
//            scr->insert(scr->end(), valuevec.begin(), valuevec.end());

//            // Sort the values and base the hist limits on the 15% and 85%
//            // percentiles, plus expand the range by 35% on each end.
//            std::sort(scr->begin(), scr->end());
//            const int ntot = scr->size();
//            const int loc15 = int(0.5+0.15*ntot);
//            const int loc85 = int(0.5+0.85*ntot);
//            const float pctile15 = (*scr)[loc15];
//            const float pctile85 = (*scr)[loc85];
//            float upper = pctile85 + (pctile85-pctile15)*0.35;
//            float lower = pctile15 - (pctile85-pctile15)*0.35;
//            if (lower == upper) {
//                lower -= 1;
//                upper += 1;
//            }

//            const size_t NBINS=100;
//            hist->reshape(NBINS, lower, upper);
//            hist->update(*scr);

//            // When the scratch data are long enough, we can stop storing the scratch
//            // values and just fix the histogram limits.
//            if (scr->size() >= NDATA_TO_FIX_HISTS)
//                scr->clear();
//        }

//        QVector<double> xqv(0);
//        QVector<double> yqv(0);
//        hist->getContents(xqv, yqv);
//        emit newDataToPlot(trace, xqv, yqv, 1.0, 1.0);
//    }
//}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Call this when the plot window changes its channels to plot.
/// \param traceNumber    Which trace is changed.
/// \param channelNumber  Which channel is now plotted in that trace.
///
void refreshPlots::changedChannel(int traceNumber, int channelNumber)
{
    if (traceNumber >= channels.size())
        return;
    channels[traceNumber] = channelNumber;
    pulseHistories[traceNumber]->clearAllData();

    //    scratch[traceNumber].clear();
    //    histograms[traceNumber]->clear();
    // don't change lastTimes[traceNumber], or we'll re-plot old data from that channel
    // when new data isn't streaming. That's not what we want.
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Change whether plotting is paused
/// \param pause_state Whether to pause plotting of new triggers.
///
void refreshPlots::pause(bool pause_state)
{
    plottingPaused = pause_state;
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Set whether to plot in fb vs err mode
/// \param evf Whether to plot in fb vs err mode
///
void refreshPlots::setErrVsFeedback(bool evf)
{
    ErrVsFeedback = evf;
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Set whether to plot in PSD mode
/// \param psd Whether to plot in PSD mode
///
void refreshPlots::setIsPSD(bool psd)
{
    if (isPSD == psd)
        return;
    toggledDFTing(psd);
    isPSD = psd;
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Set whether to plot in FFT mode
/// \param fft Whether to plot in FFT mode
///
void refreshPlots::setIsFFT(bool fft)
{
    isFFT = fft;
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Set whether to plot in histogram mode
/// \param hist Whether to plot in histogram mode
///
//void refreshPlots::setIsHistogram(bool hist)
//{
//    isHistogram = hist;
//}



///////////////////////////////////////////////////////////////////////////////
/// \brief Set whether to plot in timeseries mode
/// \param ts Whether to plot in timeseries mode
///
void refreshPlots::setIsTimeseries(bool ts)
{
    isTimeseries = ts;
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Change the analysis field to be plotted
/// \param newType  The new field.
///
void refreshPlots::setAnalysisType(enum analysisFields newType)
{
    if (analysisType == newType)
        return;

    analysisType = newType;
    for (int tracenum=0; tracenum < channels.size(); tracenum++) {
        lastSerial[tracenum] = 0;

//        scratch[tracenum].clear();
//        histograms[tracenum]->clear();
    }
}

////////////////////////////////////////////////////////////////////////////////////
// End of class refreshPlots
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
/// \brief Construct a histogram
/// \param nbins_in
/// \param lowerLimit
/// \param upperLimit
///
Histogram::Histogram(int nbins_in, double lowerLimit, double upperLimit) :
    nbins(nbins_in),
    lower(lowerLimit),
    upper(upperLimit),
    totalCounts(0),
    nUnder(0),
    nOver(0)
{
    reshape(nbins_in, lowerLimit, upperLimit);
}


////////////////////////////////////////////////////////////////////////////////////
/// \brief Histogram::reshape
/// \param nbins_in
/// \param lowerLimit
/// \param upperLimit
///
void Histogram::reshape(int nbins_in, double lowerLimit, double upperLimit)
{
    nbins = nbins_in+2;
    lower = lowerLimit;
    upper = upperLimit;
    totalCounts = nOver = nUnder = 0;
    counts.fill(0, nbins);

    if (upper > lower)
        invBinWidth = nbins_in/(upper-lower);
    else
        invBinWidth = 1.0e9;

    binCenters.resize(nbins);
    for (int i=0; i<nbins; i++)
        binCenters[i] = lower + (i-0.5)/invBinWidth;
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Clear the histogram contents
///
void Histogram::clear()
{
    nOver = nUnder = totalCounts = 0;
    counts.fill(0, nbins);
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Update the histogram by adding a single value
/// \param value  The value to insert
/// \return  Number of entries in the histogram so far
///
int Histogram::update(double value)
{
    int bin= 1 + int((value-lower) * invBinWidth);
    if (bin <= 0)
        nUnder++;
    else if (bin >= nbins-1)
        nOver++;
    else
        counts[bin]++;
    return ++totalCounts;
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Update the histogram by adding a multiple values
/// \param values  The values to insert
/// \return  Number of entries in the histogram so far
///
int Histogram::update(QVector<double> values)
{
    QVector<double>::const_iterator pv;
    int n=0;
    for (pv=values.begin(); pv != values.end(); pv++)
        n = update(*pv);
    return n;
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Update the histogram by adding a multiple values
/// \param values  The values to insert
/// \return  Number of entries in the histogram so far
///
int Histogram::update(std::vector<double> values)
{
    std::vector<double>::const_iterator pv;
    int n=0;
    for (pv=values.begin(); pv != values.end(); pv++)
        n = update(double(*pv));
    return n;
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Return handles to the bin centers and contents, for plotting.
///
/// Note that the lowest and highest bins are "dummy bins" with zero contents but
/// the expected ranges. This permits better QCustomPlot plotting of the first
/// and last true bins.
///
/// \param[out] binctrs  Center of each bin, including 1 dummy above/below
/// \param[out] contents Contents of each bin, including 1 dummy above/below
/// \return Total number of counts
///
int Histogram::getContents(QVector<double> &binctrs, QVector<double> &contents) const
{
    binctrs = binCenters;
    contents = counts;
    return totalCounts;
}

////////////////////////////////////////////////////////////////////////////////////
// End of class histogram
////////////////////////////////////////////////////////////////////////////////////

