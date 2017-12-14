#include "refreshplots.h"

///////////////////////////////////////////////////////////////////////////////
/// \file refreshplots.cpp
/// Defines refreshPlots class, to perform work at regular timed intervals
/// in its own thread (by subclassing periodicUpdater). Specifically, it updates
/// the plotted data curves on a single plotWindow object.
///


#include <iostream>
#include <QVector>

#include "periodicupdater.h"
#include "refreshplots.h"
#include "pulsehistory.h"



////////////////////////////////////////////////////////////////////////////////////
/// \brief Constructor
/// \param client_in    The active Client object
/// \param msec_period  The refresh time in milliseconds
///
refreshPlots::refreshPlots(int msec_period) :
    periodicUpdater(msec_period),
    plottingPaused(false),
    ErrVsFeedback(false),
    isPSD(false),
    isFFT(false),
//    isHistogram(false),
    analysisType(ANALYSIS_PULSE_MAX),
    time_zero(0)
{
    // Fill the list of channels to be plotted with the no-plot indicator.
    const int INITIAL_TRACES=8;
    const int DONT_PLOT = -1;
    channels.resize(INITIAL_TRACES);
    for (int i=0; i<INITIAL_TRACES; i++)
        channels[i] = DONT_PLOT;
    lastTimes.resize(INITIAL_TRACES);
    for (int i=0; i<INITIAL_TRACES; i++)
        lastTimes[i] = 0;

//    scratch.resize(INITIAL_TRACES);
//    histograms.clear();
//    for (int i=0; i<INITIAL_TRACES; i++) {
//        histograms.push_back(new Histogram);
//    }

    const int PULSES_TO_STORE=4;
    pulseHistories.reserve(INITIAL_TRACES);
    for (int i=0; i<INITIAL_TRACES; i++) {
        pulseHistories.append(new pulseHistory(PULSES_TO_STORE));
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
void refreshPlots::receiveNewData(int tracenum, const uint16_t *data, int length) {
    if (tracenum < 0 || tracenum >= pulseHistories.size())
        return;

    QVector<double> *rec = new QVector<double>(length);
    for (int i=0; i<length; i++)
        (*rec)[i]= data[i];
    pulseHistories[tracenum]->insertRecord(rec);
    std::cout << "Received record for trace " << tracenum << " (have stored "
              << pulseHistories[tracenum]->size() << ")." << std::endl;
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
//void refreshPlots::clearHistograms()
//{
//    for (unsigned int i=0; i<histograms.size(); i++) {
//        histograms[i]->clear();
//        scratch[i].clear();
//    }
//}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Called by the run loop once to draw standard (non-spectrum) plots
///
void refreshPlots::refreshStandardPlots()
{
    for (int trace=0; trace<channels.size(); trace++) {
        int channum = channels[trace];
        if (channum < 0)
            continue;

        emit newDataToPlot(trace, *pulseHistories[trace]->newestRecord());

//        xdaq::XPulseRec *PR = client->latestTriggeredPulse[channum];

        // Make sure there IS a pulse record AND that it's newer than our
        // last plot, or we don't want to plot it.
//        if (PR != NULL && PR->triggerTime() > lastTimes[trace]) {
//            lastTimes[trace] = PR->triggerTime();

//            ///\todo This is a dumb way to do things, so fix.
//            /// Notice how we're ignoring the x vector, etc etc.
//            QVector<double> _x, y;
//            const bool RAW=true;
//            PR->requestDataCopy(_x, y, RAW);
//            if (ErrVsFeedback) {
//                int chanerr = channum - 1;
//                if (client->streamDataFlag(chanerr)) {
//                    client->latestTriggeredPulseLock[chanerr].getLock();
//                    xdaq::XPulseRec *PRe = client->latestTriggeredPulse[chanerr];
//                    if (PRe != NULL) {
//                        QVector<double> _xe, ye;
//                        PRe->requestDataCopy(_xe, ye, RAW);
//                        emit newDataToPlot(trace, y, ye);
//                    }
//                }
//            } else
//                emit newDataToPlot(trace, y);
//        }
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

        // Make sure there IS a pulse record AND that it's newer than our
        // last plot, or we don't want to plot it.
//        xdaq::FFTChannel *fftchan = client->noiseRecordFFT[channum];
//        if (fftchan == NULL)
//            continue;

//        unsigned long long timecode = fftchan->plotTime();
//        if (timecode > lastTimes[trace]) {
//            lastTimes[trace] = timecode;
//            QVector<double> x, y;
//            fftchan->plotX(x);
//            fftchan->plotY(y);
//            if (isFFT) {
//                for (int i=0; i<y.size(); i++)
//                    y[i] = sqrt(y[i]);
//            }
//            emit newDataToPlot(trace, x, y);
//        }
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

//        xdaq::StreamChannel *schan = client->streamData[channum];
//        double oldest_time = double(lastTimes[trace])/1e6;
        std::vector<double> timevec, valuevec;
//        schan->getRecentAnalysis(timevec, valuevec, oldest_time, analysisType);
        unsigned int n = timevec.size();
        if (n==0)
            continue;
//        lastTimes[trace] = counter_t(timevec.back()*1e6);

        QVector<double> xqv(n);
        QVector<double> yqv(n);
        for (unsigned int i=0; i<n; i++) {
            xqv[i] = double(timevec[i] - time_zero);
            yqv[i] = double(valuevec[i]);
        }
        emit addDataToPlot(trace, xqv, yqv);
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
//        emit newDataToPlot(trace, xqv, yqv);
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

//    struct timeval tv;
//    gettimeofday(&tv, NULL);
//    counter_t now=tv.tv_sec;
//    time_zero = now - now%3600; // Round down to nearest exact hour
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Change the analysis field to be plotted
/// \param newType  The new field.
///
void refreshPlots::setAnalysisType(enum analysisFields newType)
{
    analysisType = newType;
    for (int trNum=0; trNum < channels.size(); trNum++) {
        lastTimes[trNum] = 0;
//        scratch[trNum].clear();
//        histograms[trNum]->clear();
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
    int bin= 1 + (value-lower) * invBinWidth;
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

