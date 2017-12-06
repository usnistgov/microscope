#ifndef PLOTWINDOW_H
#define PLOTWINDOW_H

///////////////////////////////////////////////////////////////////////////////
/// \file plotwindow.h
/// Declares the plotWindow object, for controlling and rendering a plot of
/// raw data.
///

#include <QMainWindow>

#include <stdint.h> // for uint16_t
#include <vector>
#include <QAction>
#include <QActionGroup>
#include <QColor>
#include <QSettings>
#include <QVector>
#include <QWidget>
#include "qcustomplot.h"


namespace Ui {
class plotWindow;
}

class refreshPlots;
class QSpinBox;
class QMouseEvent;
class QStatusBar;


///////////////////////////////////////////////////////////////////////////////
/// The standard color palette for plotting. Maybe someday this won't be fixed?
/// I'm using mostly unnamed colors so that I can make them a little darker
/// the standard ones but not as dark as the dark* ones.
///
static const QColor plotStandardColors[]={
    Qt::black,
    QColor(180,0,230,255), ///< Purple
    QColor(0,0,180,255),    ///< Blue
    QColor(0,190,190,255), ///< Cyan
    Qt::darkGreen,
    QColor(205,205,0,255), ///< Gold
    QColor(255,128,0,255), ///< Orange
    Qt::red,
    Qt::gray
};


///////////////////////////////////////////////////////////////////////////////
/// \brief The various plot types that can be rendered
///
enum plotTypeComboItems {
    PLOTTYPE_STANDARD,
    PLOTTYPE_DERIVATIVE,
    PLOTTYPE_ERRVSFB,
    PLOTTYPE_FFT,
    PLOTTYPE_PSD,
    PLOTTYPE_TIMESERIES,
    PLOTTYPE_HISTOGRAM,
    PLOTTYPE_INVALID
};

///////////////////////////////////////////////////////////////////////////////
/// \brief The various analysis fields that can be plotted
///
enum analysisFields {
    ANALYSIS_PULSE_MAX,   ///< Pulse peak value above baseline
    ANALYSIS_PUSLE_RMS,   ///< Pulse RMS value from baseline
    ANALYSIS_BASELINE,    ///< Pulse baseline (pretrigger mean)
    ANALYSIS_INVALID
};



///////////////////////////////////////////////////////////////////////////////
/// \brief The axis scaling policies
///
enum plotAxisPolicy {
    AXIS_POLICY_AUTO,
    AXIS_POLICY_EXPANDING,
    AXIS_POLICY_FIXED,
    AXIS_POLICY_INVALID
};


///////////////////////////////////////////////////////////////////////////////
/// \brief The plotWindow class, representing a single plot window.
///
class plotWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit plotWindow(QWidget *parent = 0);
    ~plotWindow();

    int chan2trace(int channum);

public slots:
    void updateQuickSelect(int nrows, int ncols);
    void newPlotTrace(int tracenum, const uint16_t *data, int nsamples);
    void newPlotTrace(int tracenum, const uint32_t *data, int nsamples);
    void newPlotTrace(int tracenum, const int16_t *data, int nsamples);
    void newPlotTrace(int tracenum, const int32_t *data, int nsamples);
    void newPlotTrace(int tracenum, const QVector<double> &data);
    void newPlotTrace(int tracenum, const QVector<double> &xdata,
                      const QVector<double> &data);
    void addPlotData(int tracenum, const QVector<double> &xdata,
                      const QVector<double> &data);
    void newSampleTime(double);
    void newRecordLengths(int,int);

signals:
    /// Signal that one trace->channel correspondence has changed.
    void oneChannelChanged(unsigned int, int);
    /// Signal that FB->err coupling is needed
    void switchedToErrVsFBPlots(void);
    /// Send a message to the console (in the main window)
    /// \param msg  The message to log.
    void sendConsoleMessage(const QString &msg);

    void startPlottingChannel(int);
    void stopPlottingChannel(int);


private:
    Ui::plotWindow *ui;                   ///< The underlying GUI form built in Qt Designer
    const static int NUM_TRACES=8;        ///< How many plot curves have their own channel selector
    std::vector<QSpinBox *> spinners;     ///< The spin boxes that control which channels are plotted
    std::vector<int> quickSelectErrChan1; ///< The lowest channel # signified by each comboBox range
    std::vector<int> quickSelectErrChan2; ///< The highest channel # signified by each comboBox range
    std::vector<int> selectedChannel;     ///< The channel number currently chosen in each spin box
    QActionGroup plotMenuActionGroup;     ///< Object that keeps plot type choices exclusive.
    QActionGroup analysisMenuActionGroup; ///< Object that keeps analysis choices exclusive.
    QActionGroup axisMenuActionGroup;     ///< Object that keeps axis choices exclusive.
    QActionGroup yaxisUnitsActionGroup;   ///< Object that keeps y-axis units choices exclusive.
    QSettings *matterSettings;            ///< Store program settings.

    int nrows;    ///< Number of rows in the current array.
    int ncols;    ///< Number of columns in the current array.
    QVector<double>  sampleIndex;        ///< Temporary object to hold [0,1,...]
    refreshPlots *refreshPlotsThread;    ///< Thread to periodically update traces.
    enum plotTypeComboItems plotType;    ///< Current plot style
    enum analysisFields analysisType;    ///< Current type of analysis to plot (histo/timeseries)
    double ms_per_sample;                ///< Scaling from sample # to ms.
    int num_presamples;                  ///< Number of pretrigger samples
    int num_samples;                     ///< Number of samples in a record
    bool preferVisibleMinMaxRange;       ///< Whether user wants min/max/range boxes visible
    bool preferYaxisRawUnits;            ///< Whether user wants raw units on y axis
    double phys_per_rawFB;               ///< Multiply raw feedback data by this to get physical units
    double phys_per_avgErr;              ///< Multiply mean error data by this to get physical units

    void startRefresh();
    void rescalePlots(QCPGraph *);
    void closeEvent(QCloseEvent *);

private slots:
    void updateSpinners(void);
    void updateQuickTypeFromErr(int);
    void updateQuickTypeFromFB(int);
    void updateQuickTypeText();
    void channelChanged(int);
    void pausePressed(bool);
    void yAxisLog(bool);
    void xAxisLog(bool);
    void updateXAxisRange(QCPRange);
    void updateYAxisRange(QCPRange);
    void typedXAxisMin(double a);
    void typedYAxisMin(double a);
    void typedXAxisMax(double b);
    void typedYAxisMax(double b);
    void typedXAxisRange(double r);
    void typedYAxisRange(double r);
    void mouseEvent(QMouseEvent *);
    void clearGraphs(void);
    void plotTypeChanged(QAction *action);
    void plotAnalysisFieldChanged(QAction *action);
    void axisRangeVisibleChanged(QAction *action);
    void yaxisUnitsChanged(QAction *action);
    void axisDoubleClicked(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*);
    void savePlot(void);
};

#endif // PLOTWINDOW_H
