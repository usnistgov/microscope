///////////////////////////////////////////////////////////////////////////////
/// \file plotwindow.cpp
/// Defines the plotWindow class, for plotting data results.
///

#include <iostream>
#include "pulserecord.h"
#include "plotwindow.h"
#include "refreshplots.h"
#include "ui_plotwindow.h"
#include "version.h"
#include "microscope.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>

Q_DECLARE_METATYPE(QCPRange)

///////////////////////////////////////////////////////////////////////////////
/// The standard color palette for plotting. Maybe someday this won't be const?
/// I'm using mostly unnamed colors so that I can make them a little darker
/// than the standard named colors but not as dark as the dark* ones.
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
/// \brief Constructor
/// \param client_in Client object that will supply the data.
/// \param parent    Qt parent widget
///
plotWindow::plotWindow(zmq::context_t *context_in, options *opt, QWidget *parent) :
    QMainWindow(parent),
    refreshPlotsThread(nullptr),
    ui(new Ui::plotWindow),
    plotMenuActionGroup(this),
    analysisMenuActionGroup(this),
    axisMenuActionGroup(this),
    yaxisUnitsActionGroup(this),
    sampleIndex(1, 0.0),
    plotType(PLOTTYPE_STANDARD),
    analysisType(ANALYSIS_PULSE_MAX),
    ms_per_sample(1),
    zmqcontext(context_in)
{
    // How many rows and columns are in the actual data:
    // set via a command-line argument.
    nrows = opt->rows;
    ncols = opt->cols;
    int nsensors = nrows * ncols;
    hasErr = opt->tdm;

    chansocket = new zmq::socket_t(*zmqcontext, ZMQ_PUB);
    try {
        chansocket->bind(CHANSUBPORT);
    } catch (zmq::error_t&) {
        delete chansocket;
        chansocket = nullptr;
    }

    setWindowFlags(Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose); // important!
    ui->setupUi(this);
    QString title("%1: microcalorimeter data plots, version %2.%3.%4");
    setWindowTitle(title.arg(opt->appname).arg(VERSION_MAJOR).arg(VERSION_MINOR).arg(VERSION_REALLYMINOR));

    // Build layout with the NUM_TRACES (8?) channel selection spin boxes
    QGridLayout *chanSpinnersLayout = new QGridLayout(nullptr);
    chanSpinnersLayout->setSpacing(3);
    ui->chanSelectLayout->insertLayout(1, chanSpinnersLayout);

    for (int i=0; i<NUM_TRACES; i++) {
        // Make a label whose color matches our color list
        QString channame("Curve %1");
        QLabel *label = new QLabel(channame.arg(char('A'+i)), this);
        QPalette palette;
        QBrush brush(plotStandardColors[i]);
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        QBrush brush1(QColor(69, 69, 69, 255));
        brush1.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush1);
        label->setPalette(palette);

        QSpinBox *box = new QSpinBox(this);
        box->setRange(0, nsensors);
        box->setSpecialValueText("--");
        box->setValue(0);
        box->setPrefix("Ch ");
        box->setAlignment(Qt::AlignRight);
        spinners.append(box);
        streamIndex.append(-1);
        connect(box, SIGNAL(valueChanged(int)), this, SLOT(channelChanged(int)));

        chanSpinnersLayout->addWidget(label, i, 0);
        chanSpinnersLayout->addWidget(box, i, 1);

        if (opt->tdm) {
            QCheckBox *check = new QCheckBox(this);
            QString tt = QString("Curve %1 use error signal").arg(char('A'+i));
            check->setToolTip(tt);
            checkers.append(check);
            connect(check, SIGNAL(toggled(bool)), this, SLOT(errStateChanged(bool)));
            chanSpinnersLayout->addWidget(check, i, 2);
        }
    }

    // Plot type: make the menu choices be exclusive.
    plotMenuActionGroup.addAction(ui->actionRaw_pulse_records);
    plotMenuActionGroup.addAction(ui->actionTime_derivatives);
    plotMenuActionGroup.addAction(ui->actionErr_vs_FB);
    plotMenuActionGroup.addAction(ui->actionFFT_sqrt_PSD);
    plotMenuActionGroup.addAction(ui->actionNoise_PSD);
    plotMenuActionGroup.addAction(ui->actionAnalysis_vs_time);
    plotMenuActionGroup.addAction(ui->actionAnalysis_histogram);
    ui->actionRaw_pulse_records->setChecked(true);
    connect(&plotMenuActionGroup, SIGNAL(triggered(QAction *)),
            this, SLOT(plotTypeChanged(QAction *)));

    // Similarly build the x-axis and y-axis policy items here.
    QComboBox *vp = ui->verticalScaleComboBox;
    vp->insertItem(AXIS_POLICY_AUTO, "Y range auto");
    vp->insertItem(AXIS_POLICY_EXPANDING, "Y range expands");
    vp->insertItem(AXIS_POLICY_FIXED, "Y range fixed");

    vp = ui->horizontalScaleComboBox;
    vp->insertItem(AXIS_POLICY_AUTO, "X range auto");
    vp->insertItem(AXIS_POLICY_EXPANDING, "X range expands");
    vp->insertItem(AXIS_POLICY_FIXED, "X range fixed");

    // Is this an err/FB (TDM) system?
    if (! hasErr) {
        ui->quickErrComboBox->hide();
        ui->quickErrLabel->hide();
        ui->quickFBLabel->setText("Quick select Chan");
        ui->actionErr_vs_FB->setDisabled(true);
    }

    // Signal-slot connections
    connect(ui->quickChanEdit, SIGNAL(textChanged()),
            this, SLOT(updateSpinners()));
    connect(ui->quickErrComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(updateQuickTypeFromErr(int)));
    connect(ui->quickFBComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(updateQuickTypeFromFB(int)));
    connect(ui->plot, SIGNAL(mouseMove(QMouseEvent*)),
            this, SLOT(mouseEvent(QMouseEvent*)));
    connect(ui->plot, SIGNAL(axisDoubleClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)),
            this, SLOT(axisDoubleClicked(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)));

    // Make analysis types be exclusive
    analysisMenuActionGroup.addAction(ui->actionPulse_mean);
    analysisMenuActionGroup.addAction(ui->actionPulse_max);
    analysisMenuActionGroup.addAction(ui->actionPulse_RMS);
    analysisMenuActionGroup.addAction(ui->actionBaseline);
    ui->actionPulse_RMS->setChecked(true);
    connect(&analysisMenuActionGroup, SIGNAL(triggered(QAction *)),
            this, SLOT(plotAnalysisFieldChanged(QAction *)));

    // Make axis range viewing options be exclusive
    axisMenuActionGroup.addAction(ui->actionHide_ranges);
    axisMenuActionGroup.addAction(ui->actionShow_edit_ranges);
    connect(&axisMenuActionGroup, SIGNAL(triggered(QAction *)),
            this, SLOT(axisRangeVisibleChanged(QAction*)));

    // Make y-axis units options be exclusive
    yaxisUnitsActionGroup.addAction(ui->actionY_axis_raw_units);
    yaxisUnitsActionGroup.addAction(ui->actionY_axis_phys_units);
    connect(&yaxisUnitsActionGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(yaxisUnitsChanged(QAction*)));

    // Set up the plot object
    QCustomPlot *pl = ui->plot;
    pl->setNotAntialiasedElements(QCP::aePlottables);
    pl->setInteraction(QCP::iRangeDrag, true);
    pl->setInteraction(QCP::iRangeZoom, true);

    // Set up x axes
    pl->xAxis->setLabel("Sample number");
    connect(ui->xminBox, SIGNAL(valueChanged(double)), this, SLOT(typedXAxisMin(double)));
    connect(ui->xmaxBox, SIGNAL(valueChanged(double)), this, SLOT(typedXAxisMax(double)));
    connect(ui->xrangeBox, SIGNAL(valueChanged(double)), this, SLOT(typedXAxisRange(double)));

    qRegisterMetaType<QCPRange>();

    // Set up top x axis; always transfer range from bottom to top axis, with fixed scale ratio
    pl->xAxis2->setLabel("Time (ms)");
    pl->xAxis2->setVisible(true);
    pl->xAxis2->setScaleRatio(pl->xAxis, ms_per_sample);
    connect(pl->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(updateXAxisRange(QCPRange)));

    // Set up y axis
    pl->yAxis->setLabel("Raw Feedback");
    connect(pl->yAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(updateYAxisRange(QCPRange)));
    connect(ui->yminBox, SIGNAL(valueChanged(double)), this, SLOT(typedYAxisMin(double)));
    connect(ui->ymaxBox, SIGNAL(valueChanged(double)), this, SLOT(typedYAxisMax(double)));
    connect(ui->yrangeBox, SIGNAL(valueChanged(double)), this, SLOT(typedYAxisRange(double)));

    // Colors
    for (int i=0; i<NUM_TRACES; i++) {
        QCPGraph *graph = pl->addGraph();
        graph->setPen(QPen(plotStandardColors[i]));
        graph->setLineStyle(QCPGraph::lsLine);
    }

    // Move settings from GUI to plot.
    xAxisLog(ui->xLogCheckBox->isChecked());
    yAxisLog(ui->yLogCheckBox->isChecked());

    // Restore relevant settings.
    mscopeSettings = new QSettings();
    preferVisibleMinMaxRange = mscopeSettings->value("plots/visibleMinMaxRange",false).toBool();
    preferYaxisRawUnits = mscopeSettings->value("plots/yaxisRawUnits", true).toBool();

    if (preferVisibleMinMaxRange)
        ui->actionShow_edit_ranges->trigger();
    else
        ui->actionHide_ranges->trigger();


    // Start the refresh loop
    startRefresh();
    updateQuickSelect(nrows, ncols);
    plotTypeChanged(ui->actionRaw_pulse_records);
    if (preferYaxisRawUnits)
        ui->actionY_axis_raw_units->trigger();
    else
        ui->actionY_axis_phys_units->trigger();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief The trace number where this channel is now being plotted (or -1 if none).
///
int plotWindow::streamnum2trace(int streamnum) {
    for (int i=0; i<NUM_TRACES; i++) {
        if (streamnum == streamIndex[i])
            return i;
    }
    return -1;
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Start the thread that refreshed the plot traces
///
void plotWindow::startRefresh(void) {
    const int PLOTPERIOD_MSEC=500;
    refreshPlotsThread = new refreshPlots(PLOTPERIOD_MSEC);

    // These connections are how the traces in the plots will be updated.
    connect(refreshPlotsThread, SIGNAL(newDataToPlot(int, const QVector<double> &, int, double)),
            this, SLOT(newPlotTrace(int, const QVector<double> &, int, double)));
    connect(refreshPlotsThread, SIGNAL(newDataToPlot(int, const QVector<double> &, const QVector<double> &, double, double)),
            this, SLOT(newPlotTrace(int, const QVector<double> &, const QVector<double> &, double, double)));
    connect(refreshPlotsThread, SIGNAL(addDataToPlot(int, const QVector<double> &, const QVector<double> &)),
            this, SLOT(addPlotData(int, const QVector<double> &, const QVector<double> &)));

    connect(ui->averageTraces, SIGNAL(toggled(bool)), refreshPlotsThread, SLOT(toggledAveraging(bool)));
    connect(ui->spinBox_nAverage, SIGNAL(valueChanged(int)), refreshPlotsThread, SLOT(nAverageChanged(int)));
    connect(this, SIGNAL(doDFT(bool)), refreshPlotsThread, SLOT(toggledDFTing(bool)));
    connect(ui->clearDataButton, SIGNAL(clicked()), refreshPlotsThread, SLOT(clearStoredData()));
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Destructor
///
plotWindow::~plotWindow()
{
    delete chansocket;
    delete ui;
}



///////////////////////////////////////////////////////////////////////////////
/// Reimplement closeEvent so we can be sure the refresh thread is stopped.
/// \param event  The QCloseEvent that causes this to close.
///
void plotWindow::closeEvent(QCloseEvent *event)
{
    delete refreshPlotsThread;
    refreshPlotsThread = nullptr;
    QMainWindow::closeEvent(event);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Render a plot onto a given trace number with implied x-axis.
/// \param tracenum  Which trace (color) this is.
/// \param ydata     The raw data vector for the y-axis
/// The x-axis data will be assumed.
///
void plotWindow::newPlotTrace(int tracenum, const QVector<double> &ydata, int pre, double mVPerArb)
{

    // The x-axis plots integers -pre to N-1-pre.
    // Make sure we have a vector of that length, properly initialized.
    const int nsamples = ydata.size();
    const int si_size = sampleIndex.size();
    sampleIndex.resize(nsamples);
    if (static_cast<int>(sampleIndex[0]) != -pre) {
        for (int i=0; i<nsamples; i++)
            sampleIndex[i] = i-pre;
    } else if (si_size < nsamples) {
        int start = si_size;
        for (int i=start; i<nsamples; i++)
            sampleIndex[i] = i-pre;
    }
    newPlotTrace(tracenum, sampleIndex, ydata, 1.0, mVPerArb);
}


///////////////////////////////////////////////////////////////////////////////
/// \brief Render a plot onto a given trace number with specified x-axis.
/// \param tracenum  Which trace (color) this is.
/// \param xdata     The raw data vector for the x-axis
/// \param ydata     The raw data vector for the y-axis
///
void plotWindow::newPlotTrace(int tracenum, const QVector<double> &xdata,
                              const QVector<double> &ydata, double xmVPerArb, double ymVPerArb)
{
    QCPGraph *graph = ui->plot->graph(tracenum);
    const int N = ydata.size();

    // Convert y and (if err-vs-FB) sometimes x data to physical units.
    if (!preferYaxisRawUnits) {

        // Scale the data by the appropriate physical/raw ratio.
        QVector<double> scaled_data(ydata);
        for (int i=0; i<N; i++)
            scaled_data[i] *= ymVPerArb;

        switch (plotType) {
            case PLOTTYPE_ERRVSFB:
            {
            QVector<double> scaled_xdata(xdata);
                for (int i=0; i<N; i++)
                    scaled_xdata[i] *= xmVPerArb;
                graph->setData(scaled_xdata, scaled_data);
                break;
            }

            case PLOTTYPE_DERIVATIVE:
                for (int i=0; i<N-1; i++)
                    scaled_data[i] = scaled_data[i+1]-scaled_data[i];
                scaled_data[N-1] = scaled_data[N-2];
                graph->setData(xdata, scaled_data);
                break;

            case PLOTTYPE_STANDARD:
            case PLOTTYPE_FFT:
            case PLOTTYPE_PSD:
            default:
                graph->setData(xdata, scaled_data);
                break;
        }

        rescalePlots(graph);
        return;
    }

    // No raw->physical scaling. Handle time derivative plots
    if (plotType == PLOTTYPE_DERIVATIVE) {
        QVector<double> derivdata(ydata);
        for (int i=0; i<N-1; i++)
            derivdata[i] = derivdata[i+1] - derivdata[i];
        derivdata[N-1] = derivdata[N-2];
        graph->setData(xdata, derivdata);
    } else
        graph->setData(xdata, ydata);
    rescalePlots(graph);
    updateXAxisRange(ui->plot->xAxis->range());
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Re-adjust the plot now that there's new data
/// \param graph  The specific data graph being adjusted
///
void plotWindow::rescalePlots(QCPGraph *graph)
{
    const int xtype = ui->horizontalScaleComboBox->currentIndex();
    if (xtype == AXIS_POLICY_AUTO) {
        ui->plot->xAxis->rescale();
    } else if (xtype == AXIS_POLICY_EXPANDING) {
        const bool EXPAND_ONLY=true;
        graph->rescaleKeyAxis(EXPAND_ONLY);
    }

    const int ytype = ui->verticalScaleComboBox->currentIndex();
    if (ytype == AXIS_POLICY_AUTO) {
        ui->plot->yAxis->rescale();
    } else if (ytype == AXIS_POLICY_EXPANDING) {
        const bool EXPAND_ONLY=true;
        graph->rescaleValueAxis(EXPAND_ONLY);
    }
    ui->plot->replot();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Add data values to a given trace number, as for time series plots.
/// \param tracenum  Which trace (color) this is.
/// \param xdata     The raw data vector for the x-axis
/// \param ydata     The raw data vector for the y-axis
///
void plotWindow::addPlotData(int tracenum, const QVector<double> &xdata,
                              const QVector<double> &ydata)
{
    if (xdata.size() == 0 && ydata.size() == 0)
        return;
    QCPGraph *graph = ui->plot->graph(tracenum);
    graph->addData(xdata, ydata);
    /// \todo Will need a way to remove data after there's too much. see graph->removeDataBefore()
    rescalePlots(graph);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Slot to be informed that the sample time has changed.
/// \param dt  The new sample time (in seconds)
///
void plotWindow::newSampleTime(double dt)
{
    if (approx_equal(dt*1000., ms_per_sample, 1e-5))
        return;
    ms_per_sample = dt*1000.;
    updateXAxisRange(ui->plot->xAxis->range());
}



///////////////////////////////////////////////////////////////////////////////
/// \brief A slot to update the channel select spin box values from the text.
///
void plotWindow::updateSpinners(void)
{
    // Convert commas/periods to spaces, replace all whitespace with single space,
    // and split on space.
    QString text = ui->quickChanEdit->toPlainText();
    if (text.length() < 0)
        return;
    text.replace(","," ");
    text.replace("."," ");
    text = text.simplified();
    QStringList splitText = text.split(" ");

    if (splitText.isEmpty())
        return;

    int spin_id = 0;
    QStringList::iterator pstr;
    for (spin_id = 0, pstr=splitText.begin();
         pstr != splitText.end(); pstr++) {
        if (spin_id >= NUM_TRACES)
            break;
        bool ok;
        bool isErr = false;
        if (pstr->at(0) == 'e') {
            isErr = true;
            pstr->remove(0, 1);
        }
        int i = pstr->toInt(&ok, 10);
        if (!ok)
            i = 0;
        if (hasErr)
            checkers[spin_id]->setChecked(isErr);
        spinners[spin_id++]->setValue(i);
    }
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Update the quick select combo boxes using the new array geometry.
/// \param nrows_in The number of rows now
/// \param ncols_in The number of columns now
///
void plotWindow::updateQuickSelect(int nrows_in, int ncols_in)
{
    quickSelectChanMin.clear();
    quickSelectChanMax.clear();
    quickSelectChanMin.append(-1);
    quickSelectChanMax.append(-1);
    quickSelectFBTexts.append(QString(""));
    quickSelectErrTexts.append(QString(""));
    ui->quickFBComboBox->clear();
    ui->quickErrComboBox->clear();
    ui->quickFBComboBox->addItem("");
    ui->quickErrComboBox->addItem("");
    nrows = nrows_in;
    ncols = ncols_in;

    if (nrows <= 0 || ncols <= 0)
        return;

    const int entries_per_column = (nrows-1+NUM_TRACES)/NUM_TRACES;
    const int entries_per_label = (nrows-1+entries_per_column)/entries_per_column;

    for (int c=0; c<ncols; c++)
        for (int e=0; e<entries_per_column; e++) {
            QString text("%1 %2-%3 (col %4)");
            int c2 = c*nrows+e*entries_per_label + 1;
            int c3 = c2+entries_per_label - 1;
            // Don't let the last channel in the list be selected from column c+1!
            if (c3 > (c+1)*nrows)
                c3 = (c+1)*nrows;
            quickSelectChanMin.append(c2);
            quickSelectChanMax.append(c3);
            QString fb, err;
            for (int i=c2; i<=c3; i++) {
                fb.append(QString("%1,").arg(i));
                err.append(QString("e%1,").arg(i));
            }
            fb.chop(1);  // remove trailing comma
            err.chop(1);
            // Pad to length 8
            for (int i=0; i<NUM_TRACES-(c3-c2+1); i++) {
                fb.append(",-");
                err.append(",-");
            }
            quickSelectFBTexts.append(fb);
            quickSelectErrTexts.append(err);
            ui->quickFBComboBox->addItem(text.arg("Ch").arg(c2).arg(c3).arg(c));
            ui->quickErrComboBox->addItem(text.arg("Err").arg(c2).arg(c3).arg(c));
        }
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Use entry in quickErrComboBox to set the curve A-H values
/// \param index  Index into the comboBox entries
///
void plotWindow::updateQuickTypeFromErr(int index)
{
    if (index <= 0)
        return;
    ui->quickChanEdit->setPlainText(quickSelectErrTexts[index]);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Use entry in quickFBComboBox to set the curve A-H values
/// \param index  Index into the comboBox entries
///
void plotWindow::updateQuickTypeFromFB(int index)
{
    if (index <= 0)
        return;
    ui->quickChanEdit->setPlainText(quickSelectFBTexts[index]);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Slot to update the quick-type text list of channels when the
/// selected channels are changed.
///
void plotWindow::updateQuickTypeText(void)
{
    QString text;
    for (QVector<QSpinBox *>::iterator box=spinners.begin();
         box != spinners.end(); box++) {
        int c = (*box)->value();
        if (c>0) {
            if ((*box)->text().startsWith("Err"))
                text += QString("e%1,").arg(c);
            else
                text += QString("%1,").arg(c);
        } else {
            text += "-,";
        }
    }
    text.chop(1); // remove trailing comma
    QString existingText = ui->quickChanEdit->toPlainText();
    if (text != existingText) {
        ui->quickChanEdit->setPlainText(text);
        for (int i=1; i<quickSelectFBTexts.size(); i++) {
            if (text == quickSelectFBTexts[i]) {
                ui->quickFBComboBox->setCurrentIndex(i);
                ui->quickErrComboBox->setCurrentIndex(0);
                return;
            }
        }
        for (int i=1; i<quickSelectErrTexts.size(); i++) {
            if (text == quickSelectErrTexts[i]) {
                ui->quickErrComboBox->setCurrentIndex(i);
                ui->quickFBComboBox->setCurrentIndex(0);
                return;
            }
        }
        ui->quickFBComboBox->setCurrentIndex(0);
        ui->quickErrComboBox->setCurrentIndex(0);
    }
}


///////////////////////////////////////////////////////////////////////////////
/// \brief Set up to subscribe to a new stream.
///
/// \param tracenum  The trace number on the plot window.
/// \param newStreamIndex The streamIndex to subscribe to.
///
void plotWindow::subscribeStream(int tracenum, int newStreamIndex) {
    int oldStreamIndex = streamIndex[tracenum];
    streamIndex[tracenum] = newStreamIndex;
//    std::cout << "Trace " << tracenum << " index: " << oldStreamIndex << "->" <<
//                 newStreamIndex << std::endl;

    // Signal to dataSubscriber
    if (newStreamIndex >= 0) {
        char text[50];
        snprintf(text, 50, "add %d", newStreamIndex);
        zmq::message_t msg(text, strlen(text));
        chansocket->send(msg);
    }

    // Now unsubscribe the previous channel, first checking that it's not
    // on the list of streams still being used (by another trace).
    for (int j=0; j<streamIndex.size(); j++) {
        if (oldStreamIndex == streamIndex[j]) {
            return;
        }
    }
    if (oldStreamIndex >= 0) {
        char text[50];
        snprintf(text, 50, "rem %d", oldStreamIndex);
        zmq::message_t msg(text, strlen(text));
        chansocket->send(msg);
    }
}


///////////////////////////////////////////////////////////////////////////////
/// \brief Slot when one of the channel-choosing spin boxes changes value.
///
/// This method must identify which spin box it was, using Qt sender().
///
/// \param newChan  The new channel
///
void plotWindow::channelChanged(int newChan)
{
    QSpinBox *box = static_cast<QSpinBox *>(sender());
    for (int i=0; i<spinners.size(); i++) {
        if (spinners[i] == box) {
            bool isErr = false;
            int newStreamIndex = newChan - 1;
            if (hasErr) {
                newStreamIndex *= 2;
                isErr = checkers[i]->isChecked();
                if (!isErr)
                    newStreamIndex++;
            }
            subscribeStream(i, newStreamIndex);
            updateQuickTypeText();
            refreshPlotsThread->changedChannel(i, newStreamIndex);
            QCPGraph *graph = ui->plot->graph(i);
            QVector<double> empty;
            graph->setData(empty, empty);
            return;
        }
    }
    std::cout << "Failed plotWindow::channelChanged" <<std::endl;
}




///////////////////////////////////////////////////////////////////////////////
/// \brief Slot when one of the error-state-choosing check boxes changes value.
///
/// This method must identify which check box it was, using Qt sender().
///
/// \param newChan  The new channel
///
void plotWindow::errStateChanged(bool checked)
{
    QCheckBox *box = static_cast<QCheckBox *>(sender());
    for (int i=0; i<checkers.size(); i++) {
        if (checkers[i] == box) {
            int oldStreamIndex = streamIndex[i];
            int newStreamIndex = oldStreamIndex - oldStreamIndex % 2;
            if (!checked)
                newStreamIndex++;
            if (newStreamIndex == oldStreamIndex)
                return;

            QSpinBox *sb = spinners[i];
            if (checked) {
                sb->setPrefix("Err ");
            } else{
                sb->setPrefix("Ch ");
            }

            subscribeStream(i, newStreamIndex);
            updateQuickTypeText();
            refreshPlotsThread->changedChannel(i, newStreamIndex);
            QCPGraph *graph = ui->plot->graph(i);
            QVector<double> empty;
            graph->setData(empty, empty);
            return;
        }
    }
    std::cout << "Failed plotWindow::channelChanged" <<std::endl;
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Slot for pressing the pause button
/// \param pause_state  Whether set to pressed (for pause) or not.
///
void plotWindow::pausePressed(bool pause_state)
{
    refreshPlotsThread->pause(pause_state);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Slot for pressing the log-x-axis checkbox.
/// \param checked  Whether set to log (checked) or linear (unchecked)
///
void plotWindow::xAxisLog(bool checked)
{
    QCPAxis::ScaleType st;
    if (checked)
        st = QCPAxis::stLogarithmic;
    else
        st= QCPAxis::stLinear;
    ui->plot->xAxis->setScaleType(st);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Slot for pressing the log-y-axis checkbox.
/// \param checked  Whether set to log (checked) or linear (unchecked)
///
void plotWindow::yAxisLog(bool checked)
{
    QCPAxis::ScaleType st;
    if (checked)
        st = QCPAxis::stLogarithmic;
    else
        st= QCPAxis::stLinear;
    ui->plot->yAxis->setScaleType(st);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief A slot to fix the upper x axis range (ms) when the lower (sample #)
/// changes.
///
/// Note that this will still happen but have no visible effect when we're
/// looking at a plot where the xAxis2 (top axis) is not shown, as with power
/// spectra, histograms, or analysis timeseries.
///
/// \param newrange  The new x-axis range for the lower (sample #) axis.
///
void plotWindow::updateXAxisRange(QCPRange newrange)
{
    ui->xminBox->setValue(newrange.lower);
    ui->xmaxBox->setValue(newrange.upper);
    ui->xrangeBox->setValue(newrange.size());

    if (ms_per_sample > 0) {
        newrange *= ms_per_sample;
        ui->plot->xAxis2->setRange(newrange);
    }
}



///////////////////////////////////////////////////////////////////////////////
/// \brief A slot to fix the upper y axis range text boxes when the
/// range changes.
///
///
/// \param newrange  The new y-axis range for the left (arb-units) axis.
///
void plotWindow::updateYAxisRange(QCPRange newrange)
{
    ui->yminBox->setValue(newrange.lower);
    ui->ymaxBox->setValue(newrange.upper);
    ui->yrangeBox->setValue(newrange.size());
}



///////////////////////////////////////////////////////////////////////////////
/// \brief A slot to fix the x axis range when the minimum is changed.
///
/// \param a  The new x-axis minimum
///
void plotWindow::typedXAxisMin(double a)
{
    const double b = ui->xmaxBox->value();
    ui->plot->xAxis->setRange(QCPRange(a,b));
    ui->plot->replot();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief A slot to fix the y axis range when the minimum is changed.
///
/// \param a  The new y-axis minimum
///
void plotWindow::typedYAxisMin(double a)
{
    const double b = ui->ymaxBox->value();
    ui->plot->yAxis->setRange(QCPRange(a,b));
    ui->plot->replot();
}




///////////////////////////////////////////////////////////////////////////////
/// \brief A slot to fix the x axis range when the maximum is changed.
///
/// \param b  The new x-axis maximum
///
void plotWindow::typedXAxisMax(double b)
{
    const double a = ui->xminBox->value();
    ui->plot->xAxis->setRange(QCPRange(a,b));
    ui->plot->replot();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief A slot to fix the y axis range when the maximum is changed.
///
/// \param a  The new y-axis maximum
///
void plotWindow::typedYAxisMax(double b)
{
    const double a = ui->yminBox->value();
    ui->plot->yAxis->setRange(QCPRange(a,b));
    ui->plot->replot();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief A slot to fix the upper y axis range text boxes when the
/// range changes.
///
/// \param newrange  The new x-axis range for the left (arb-units) axis.
///
void plotWindow::typedXAxisRange(double r)
{
    double a = ui->xminBox->value();
    double b = ui->xmaxBox->value();
    const double mid = 0.5*(b+a);
    a = mid-0.5*r;
    b = mid+0.5*r;
    ui->plot->xAxis->setRange(QCPRange(a,b));
    ui->plot->replot();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief A slot to fix the upper y axis range text boxes when the
/// range changes.
///
/// \param newrange  The new y-axis range for the left (arb-units) axis.
///
void plotWindow::typedYAxisRange(double r)
{
    double a = ui->yminBox->value();
    double b = ui->ymaxBox->value();
    const double mid = 0.5*(b+a);
    a = mid-0.5*r;
    b = mid+0.5*r;
    ui->plot->yAxis->setRange(QCPRange(a,b));
    ui->plot->replot();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Clear all graphed data
///
void plotWindow::clearGraphs(void)
{
    // Clear existing data
    QCustomPlot *pl = ui->plot;
    const int ngr = pl->graphCount();
    for (int i=0; i<ngr; i++){
        QVector<double> empty;
        pl->graph(i)->setData(empty, empty);
    }
    pl->replot();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief A slot to handle change in the plot type.
/// \param action  The new plot type (indicated by the menu value)
///
void plotWindow::plotTypeChanged(QAction *action)
{
    clearGraphs();

    bool is_xvsy = false;
    bool is_fft_or_psd = false;
    if (action == ui->actionRaw_pulse_records) {
        plotType = PLOTTYPE_STANDARD;
    } else if (action == ui->actionTime_derivatives) {
        plotType = PLOTTYPE_DERIVATIVE;
    } else if (action == ui->actionErr_vs_FB) {
        plotType = PLOTTYPE_ERRVSFB;
        is_xvsy = true;
        emit switchedToErrVsFBPlots();
    } else if (action == ui->actionFFT_sqrt_PSD) {
        plotType = PLOTTYPE_FFT;
        is_fft_or_psd = true;
    } else if (action == ui->actionNoise_PSD) {
        plotType = PLOTTYPE_PSD;
        is_fft_or_psd = true;
    } else if (action == ui->actionAnalysis_vs_time) {
        plotType = PLOTTYPE_TIMESERIES;
    } else if (action == ui->actionAnalysis_histogram) {
        plotType = PLOTTYPE_HISTOGRAM;
    } else
        return;

    refreshPlotsThread->setErrVsFeedback(is_xvsy);
    ui->xLogCheckBox->setChecked(is_fft_or_psd);
    ui->yLogCheckBox->setChecked(is_fft_or_psd);
    refreshPlotsThread->setIsFFT(plotType == PLOTTYPE_FFT);
    refreshPlotsThread->setIsPSD(plotType == PLOTTYPE_PSD);
    refreshPlotsThread->setIsTimeseries(plotType == PLOTTYPE_TIMESERIES);
    emit doDFT(is_fft_or_psd);
//    refreshPlotsThread->setIsHistogram(plotType == PLOTTYPE_HISTOGRAM);

    bool scatter=false, line=false, histogram=false;

    QCustomPlot *pl = ui->plot;
    switch (plotType) {
    case PLOTTYPE_ERRVSFB:
        if (preferYaxisRawUnits) {
            pl->xAxis->setLabel("Raw Feedback");
            pl->yAxis->setLabel("Raw Error");
        } else {
            pl->xAxis->setLabel("Feedback (mV)");
            pl->yAxis->setLabel("Error (mV)");
        }
        pl->xAxis2->setVisible(false);
        line = true;
        break;

    case PLOTTYPE_PSD:
        if (preferYaxisRawUnits) {
            pl->yAxis->setLabel("Power spectral density (arbs^2/Hz)");
        } else {
            pl->yAxis->setLabel("Power spectral density (mV^2/Hz)");
        }
        pl->xAxis->setLabel("Frequency (Hz)");
        pl->xAxis2->setVisible(false);
        line = true;
        break;

    case PLOTTYPE_FFT:
        if (preferYaxisRawUnits) {
            pl->yAxis->setLabel("FFT magnitude (arbs/sqrt[Hz])");
        } else {
            pl->yAxis->setLabel("FFT magnitude (mV/sqrt[Hz])");
        }
        pl->xAxis->setLabel("Frequency (Hz)");
        pl->xAxis2->setVisible(false);
        line = true;
        break;

    case PLOTTYPE_TIMESERIES:
        switch (analysisType) {
        case ANALYSIS_BASELINE:
            pl->yAxis->setLabel("Pretrigger mean (arbs)");
            break;
        case ANALYSIS_PULSE_MAX:
            pl->yAxis->setLabel("Pulse max value (arbs)");
            break;
        case ANALYSIS_PULSE_MEAN:
            pl->yAxis->setLabel("Pulse average value (arbs)");
            break;
        case ANALYSIS_PULSE_RMS:
        default:
            pl->yAxis->setLabel("Pulse RMS value (arbs)");
            break;
        }

        pl->xAxis->setLabel("Time (sec since previous hour)");
        pl->xAxis2->setVisible(false);
        scatter = true;
        break;

    case PLOTTYPE_HISTOGRAM:
        pl->yAxis->setLabel("Records per bin");
        pl->xAxis->setLabel("Pulse height (arbs)");
        pl->xAxis2->setVisible(false);
        histogram = true;
        break;

    case PLOTTYPE_DERIVATIVE:
        if (preferYaxisRawUnits)
            pl->yAxis->setLabel("Raw units / sample");
        else
            pl->yAxis->setLabel("Millivolts / sample");
        pl->xAxis->setLabel("Sample number");
        pl->xAxis2->setVisible(true);
        line = true;
        break;

    case PLOTTYPE_STANDARD:
    default:
        if (preferYaxisRawUnits)
            pl->yAxis->setLabel("Raw units");
        else
            pl->yAxis->setLabel("Millivolts");
        pl->xAxis->setLabel("Sample number");
        pl->xAxis2->setVisible(true);
        line = true;
        break;

    }

    // Require odd channel # in Err vs FB mode
    if (is_xvsy) {
        for (int i=0; i<spinners.size(); i++) {
            QSpinBox *box = spinners[i];
            box->setPrefix("Ch ");
            checkers[i]->setChecked(false);
        }
        for (int i=0; i<checkers.size(); i++) {
            checkers[i]->setEnabled(false);
        }
        ui->quickErrComboBox->setCurrentIndex(0);
        ui->quickErrComboBox->setEnabled(false);
        updateQuickTypeText();
    } else {
        for (int i=0; i<checkers.size(); i++) {
            checkers[i]->setEnabled(hasErr);
        }
        ui->quickErrComboBox->setEnabled(hasErr);
    }

    if (scatter) {
        for (int i=0; i<pl->graphCount(); i++ ) {
            pl->graph(i)->setLineStyle(QCPGraph::lsNone);
            QCPScatterStyle myScatStyle;
            myScatStyle.setShape(QCPScatterStyle::ssDisc);
            myScatStyle.setSize(3);
            pl->graph(i)->setScatterStyle(myScatStyle);
        }
    }
    if (line) {
        for (int i=0; i<pl->graphCount(); i++ ) {
            pl->graph(i)->setLineStyle(QCPGraph::lsLine);
            pl->graph(i)->setScatterStyle(QCPScatterStyle::ssNone);
        }
    }
    if (histogram) {
        for (int i=0; i<pl->graphCount(); i++ ) {
            pl->graph(i)->setLineStyle(QCPGraph::lsStepCenter);
            pl->graph(i)->setScatterStyle(QCPScatterStyle::ssNone);
        }
    }
}



///////////////////////////////////////////////////////////////////////////////
/// \brief The Analyzed Values menu choice was changed
/// \param action  The QAction that is now selected
///
void plotWindow::plotAnalysisFieldChanged(QAction *action)
{
    if (action == ui->actionPulse_mean) {
        analysisType = ANALYSIS_PULSE_MEAN;
    } else if (action == ui->actionPulse_max) {
        analysisType = ANALYSIS_PULSE_MAX;
    } else if (action == ui->actionPulse_RMS) {
        analysisType = ANALYSIS_PULSE_RMS;
    } else if (action == ui->actionBaseline) {
        analysisType = ANALYSIS_BASELINE;
    }

    // Reset the plot type to be analysis vs time when the user chooses any
    // analysis type, because otherwise it's
    // annoying and confusing.  Exception when already in histogram mode.
    if (plotMenuActionGroup.checkedAction() != ui->actionAnalysis_histogram) {
        ui->actionAnalysis_vs_time->setChecked(true);
        plotTypeChanged(ui->actionAnalysis_vs_time);
    }
    refreshPlotsThread->setAnalysisType(analysisType);
    clearGraphs();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief The y-axis units menu choice was changed
/// \param action  The QAction that is now selected
///
void plotWindow::yaxisUnitsChanged(QAction *action)
{
    if (action == ui->actionY_axis_raw_units) {
        preferYaxisRawUnits = true;
    } else if (action == ui->actionY_axis_phys_units) {
        preferYaxisRawUnits = false;
    } else
        return;

    // Save this choice so that future windows can be brought up in this state again.
    mscopeSettings->setValue("plots/yaxisRawUnits", preferYaxisRawUnits);
    plotTypeChanged( plotMenuActionGroup.checkedAction() );
    ui->plot->replot();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief The Axis menu choice was changed
/// \param action  The QAction that is now selected
///
void plotWindow::axisRangeVisibleChanged(QAction *action)
{
    if (action == ui->actionHide_ranges) {
        ui->minMaxRangeWidget->hide();
        preferVisibleMinMaxRange = false;
    } else if (action == ui->actionShow_edit_ranges) {
        ui->minMaxRangeWidget->show();
        preferVisibleMinMaxRange = true;
    } else
        return;

    // Save this choice so that future windows can be brought up in this state again.
    mscopeSettings->setValue("plots/visibleMinMaxRange", preferVisibleMinMaxRange);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Handle a mouse move event by updating the plot position in the
/// status bar.
/// \param m  The mouse event (contains position info)
///
void plotWindow::mouseEvent(QMouseEvent *m)
{
    int x=m->x(), y=m->y();
    QCustomPlot *pl = ui->plot;
    QString label;
    switch (plotType) {
    case PLOTTYPE_ERRVSFB:
        label = QString("(FB=%1, Err=%2)")
                .arg(pl->xAxis->pixelToCoord(x))
                .arg(pl->yAxis->pixelToCoord(y));
        break;

    case PLOTTYPE_FFT:
    case PLOTTYPE_PSD:
        label = QString("(%1 Hz, y=%2)")
                .arg(pl->xAxis->pixelToCoord(x))
                .arg(pl->yAxis->pixelToCoord(y));
        break;

    case PLOTTYPE_TIMESERIES: // analysis
        label = QString("(%1 sec, y=%2 <varies>)")
                .arg(pl->xAxis->pixelToCoord(x))
                .arg(pl->yAxis->pixelToCoord(y));
        break;

    case PLOTTYPE_HISTOGRAM:
        label = QString("(value=%1 <varies>, y=%2 pulses)")
                .arg(pl->xAxis->pixelToCoord(x))
                .arg(pl->yAxis->pixelToCoord(y));
        break;

    case PLOTTYPE_STANDARD:
    case PLOTTYPE_DERIVATIVE:
    default:
        label = QString("(%1 samp = %2 ms, y=%3)")
                .arg(pl->xAxis->pixelToCoord(x))
                .arg(pl->xAxis2->pixelToCoord(x))
                .arg(pl->yAxis->pixelToCoord(y));
        break;

    }

    ui->statusbar->showMessage(label);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Slot called when any axis is double-clicked. This means user wants
/// to re-scale that axis.
/// \param axis  The axis object that was double-clicked.
///
void plotWindow::axisDoubleClicked(QCPAxis *axis, QCPAxis::SelectablePart,
                                   QMouseEvent *)
{
    if (axis == ui->plot->xAxis2)
        ui->plot->xAxis->rescale();
    else
        axis->rescale();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Slot called when the user wants to save a plot
///
void plotWindow::savePlot()
{
    QSettings *settings = new QSettings();
    QString startingDir = settings->value("lastPlotImageFile","").toString();
    QString filename = QFileDialog::getSaveFileName(
                this, "Save to image file (*.pdf, *.png, or *.jpg)",
                startingDir, "Images (*.pdf *.png *.jpg)");
    if (filename.length() == 0) {
        std::cout << "No file selected." << std::endl;
        return;
    }

    bool success=false;
    if (filename.endsWith("pdf", Qt::CaseInsensitive)) {
        const QString creator =
                QString("Microscope microcalorimeter plotting program v%1.%2.%3")
                .arg(VERSION_MAJOR).arg(VERSION_MINOR).arg(VERSION_REALLYMINOR);
        const QString title("Screen capture of a plot");
        success = ui->plot->savePdf(filename, 0, 0, QCP::epAllowCosmetic, creator, title);
    } else if (filename.endsWith("png", Qt::CaseInsensitive)) {
        success = ui->plot->savePng(filename);
    } else if (filename.endsWith("jpg", Qt::CaseInsensitive)) {
       success =  ui->plot->saveJpg(filename);
    }

    if (success) {
        settings->setValue("lastPlotImageFile", filename);
        QString msg=QString("Saved image to %1").arg(filename);
        sendConsoleMessage(msg);
    } else
        sendConsoleMessage("Failed to save image file.");

}

///////////////////////////////////////////////////////////////////////////////

void plotWindow::terminate() {
    std::cout << "We are terminating the plot window" << std::endl;
    close();
}
