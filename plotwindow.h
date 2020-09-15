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
#include <QCheckBox>
#include <QColor>
#include <QSettings>
#include <QString>
#include <QVector>
#include <QWidget>
#include "options.h"
#include "qcustomplot.h"
#include  <zmq.hpp>

namespace Ui {
class plotWindow;
}

class refreshPlots;
class pulseRecord;
class QSpinBox;
class QMouseEvent;
class QStatusBar;


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
    ANALYSIS_PULSE_RMS,   ///< Pulse RMS value from baseline
    ANALYSIS_PULSE_MEAN,  ///< Pulse mean value above baseline
    ANALYSIS_PULSE_MAX,   ///< Pulse peak value above baseline
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
    explicit plotWindow(zmq::context_t *zmqcontext, options *opt, QWidget *parent = 0);
    ~plotWindow();

    int streamnum2trace(int streamnum);

    refreshPlots *refreshPlotsThread;    ///< Thread to periodically update traces.

public slots:
    void updateQuickSelect(int nrows, int ncols);
    void newPlotTrace(int tracenum, const QVector<double> &ydata, int pre, double mVPerArb);
    void newPlotTrace(int tracenum, const QVector<double> &xdata,
                      const QVector<double> &ydata, double xmVPerArb, double ymVPerArb);
    void addPlotData(int tracenum, const QVector<double> &xdata,
                      const QVector<double> &data);
    void newSampleTime(double);

signals:
    /// Signal that one trace->channel correspondence has changed.
    void oneChannelChanged(unsigned int, int);
    /// Signal that FB->err coupling is needed
    void switchedToErrVsFBPlots(void);
    /// Send a message to the console (in the main window)
    /// \param msg  The message to log.
    void sendConsoleMessage(const QString &msg);
    void doDFT(bool);

private:
    Ui::plotWindow *ui;                   ///< The underlying GUI form built in Qt Designer
    const static int NUM_TRACES=8;        ///< How many plot curves have their own channel selector
    QVector<QSpinBox *> spinners;         ///< The spin boxes that control which channels are plotted
    QVector<QCheckBox *> checkers;        ///< The check boxes that control which traces are error traces
    QVector<int> quickSelectChanMin;      ///< The lowest channel # signified by each comboBox range
    QVector<int> quickSelectChanMax;      ///< The highest channel # signified by each comboBox range
    QVector<QString> quickSelectFBTexts;  ///< The text signified by each comboBox range
    QVector<QString> quickSelectErrTexts; ///< The text signified by each comboBox range
    QVector<int> streamIndex;             ///< The data stream index that selectedChannel corresponds to
    QActionGroup plotMenuActionGroup;     ///< Object that keeps plot type choices exclusive.
    QActionGroup analysisMenuActionGroup; ///< Object that keeps analysis choices exclusive.
    QActionGroup axisMenuActionGroup;     ///< Object that keeps axis choices exclusive.
    QActionGroup yaxisUnitsActionGroup;   ///< Object that keeps y-axis units choices exclusive.
    QSettings *mscopeSettings;            ///< Store program settings.

    int nrows;                            ///< Number of rows in the current microcal array.
    int ncols;                            ///< Number of columns in the current microcal array.
    QVector<double>  sampleIndex;         ///< Object to hold [-nPre,1-nPre,...N-2-nPre]
    enum plotTypeComboItems plotType;     ///< Current plot style
    enum analysisFields analysisType;     ///< Current type of analysis to plot (histo/timeseries)
    double ms_per_sample;                 ///< Scaling from sample # to ms.
    bool preferVisibleMinMaxRange;        ///< Whether user wants min/max/range boxes visible
    bool preferYaxisRawUnits;             ///< Whether user wants raw units on y axis
    bool hasErr;                          ///< Whether this source has error/FB channels
    zmq::context_t *zmqcontext;
    zmq::socket_t *chansocket;

    void startRefresh();
    void rescalePlots(QCPGraph *);
    void closeEvent(QCloseEvent *);
    void subscribeStream(int tracenum, int newStreamIndex);

private slots:
    void updateSpinners(void);
    void updateQuickTypeFromErr(int);
    void updateQuickTypeFromFB(int);
    void updateQuickTypeText();
    void channelChanged(int);
    void errStateChanged(bool);
    void pausePressed(bool);
    void yAxisLog(bool);
    void xAxisLog(bool);
    void forcePositiveXAxisRange();
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
    void terminate(void);
};

#endif // PLOTWINDOW_H
