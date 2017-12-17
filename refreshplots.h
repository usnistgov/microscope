#ifndef REFRESHPLOTS_H
#define REFRESHPLOTS_H

///////////////////////////////////////////////////////////////////////////////
/// \file refreshplots.h
/// Declares the refreshPlots class, which perform tasks in private threads
/// at predetermined intervals to update a single plotWindow.
///

#include <stdint.h>
#include <deque>
#include <vector>
#include "periodicupdater.h"
#include "plotwindow.h"

class plotWindow;
class pulseHistory;
class FFTMaster;

///////////////////////////////////////////////////////////////////////////////
/// \brief The Histogram class histograms data with fixed, equal bin spacings.
///
/// Careful! For purposes of improved plotting in QCustomPlot, the two vectors
/// counts (i.e., bin contents) and binCenters have an extra bin above and below
/// the true bins. The extras will always have zero contents
///
class Histogram {
public:
    Histogram(int nbins_in=1, double lowerLimit=0.0, double upperLimit=1.0);

    void reshape(int nbins_in=1, double lowerLimit=0.0, double upperLimit=1.0);
    int update(double value);
    int update(std::vector<double> values);
    int update(QVector<double> values);
    int getContents(QVector<double> &binctrs, QVector<double> &contents) const;
    void clear();
    ///\return The total number of counts entered so far.
    size_t entries() const {return totalCounts;}


private:
    int     nbins;         ///< How many bins are in the histogram
    double  lower;         ///< Lower limit of lowest bin
    double  upper;         ///< Upper limit of highest bin
    double  invBinWidth;   ///< Inverse of the bin width (inverse turns divides into multiplies)
    QVector<double> counts;///< Bin contents
    QVector<double> binCenters; ///< Center of each bin
    int totalCounts;       ///< Total number of entries
    int nUnder;            ///< Number of entries that underflowed (i.e. are < lower)
    int nOver;             ///< Number of entries that overflowed (i.e. are > upper)
};



///////////////////////////////////////////////////////////////////////////////
/// \brief The refreshPlots class is a timed repeating thread whose
/// work is to refresh all curves in all plotWindow objects
///
class refreshPlots : public periodicUpdater
{
    Q_OBJECT

public:
    refreshPlots(int period_msec);
    virtual ~refreshPlots();

    void changedChannel(int traceNumber, int channelNumber);
    void pause(bool);
    void setErrVsFeedback(bool evf);
    void setIsPSD(bool psd);
    void setIsFFT(bool fft);
    void setIsTimeseries(bool ts);
    void setAnalysisType(analysisFields newType);
    //    void setIsHistogram(bool hist);

signals:
    /// Signal that a QVector is ready to plot
    void newDataToPlot(int channum, const QVector<double> &data);

    /// Signal that a y vs x pair of QVectors are ready to plot
    void newDataToPlot(int channum, const QVector<double> &xdata,
                       const QVector<double> &ydata);

    /// Signal that additional data in an (y vs x) pair of QVectors are ready to plot
    void addDataToPlot(int channum, const QVector<double> &xdata,
                       const QVector<double> &ydata);

public slots:
    virtual void workQuantum(void);
    void receiveNewData(int tracenum, const uint16_t *data, int length, int presamples);
    void toggledAveraging(bool doAvg);
    void toggledDFTing(bool dft);
    void newSampleTime(double);

    //    void clearHistograms(void);
    //    void receiveNewData(int channum, const QVector<double> &xdata,
    //                       const QVector<double> &ydata);

private:
    double ms_per_sample;             ///< Scaling from sample # to ms.
    QVector<int> channels;            ///< The channel for each trace [0,N-1]
    QVector<int> lastSerial;          ///< The serial # of last record plotted (one per trace).
    bool plottingPaused;              ///< Is this refresher paused?
    bool ErrVsFeedback;               ///< Are we plotting Err vs FB mode?
    bool isPSD;                       ///< Are we plotting power spectral density?
    bool isFFT;                       ///< Are we plotting FFT (sqrt of PSD)?
    bool isTimeseries;                ///< Are we plotting a timeseries?
    bool averaging;                   ///< Do we average stored data before plotting
    bool doingDFT;                    ///< Do we do DFT of data?
    enum analysisFields analysisType; ///< What analysis field to plot?
    double time_zero;                 ///< In a timeseries, what time is plotted as t=0?

    QVector<pulseHistory *> pulseHistories;
    FFTMaster *fftMaster;
    QVector<double> frequencies;

    void refreshSpectrumPlots(void);
    void refreshStandardPlots(void);
    void refreshTimeseriesPlots(void);

    //    std::vector<Histogram *> histograms;       ///< Histograms to be used when histogramming analysis
    //    std::vector<std::vector<double> > scratch;  ///< Scratch space for pre-histogrammed data
    //    bool isHistogram;                 ///< Are we plotting histograms?
    //    void refreshHistograms(void);
};

#endif // REFRESHPLOTS_H
