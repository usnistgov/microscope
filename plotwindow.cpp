///////////////////////////////////////////////////////////////////////////////
/// \file plotwindow.cpp
/// Defines the plotWindow class, for plotting data results.
///

#include <iostream>
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


///////////////////////////////////////////////////////////////////////////////
/// \brief Constructor
/// \param client_in Client object that will supply the data.
/// \param parent    Qt parent widget
///
plotWindow::plotWindow(zmq::context_t *context_in, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::plotWindow),
    plotMenuActionGroup(this),
    analysisMenuActionGroup(this),
    axisMenuActionGroup(this),
    yaxisUnitsActionGroup(this),
    sampleIndex(),
    refreshPlotsThread(NULL),
    plotType(PLOTTYPE_STANDARD),
    analysisType(ANALYSIS_PULSE_MAX),
    ms_per_sample(1),
    num_presamples(0),
    phys_per_rawFB(1000.0/16384.),
    phys_per_avgErr(1000.0/4096.),
    zmqcontext(context_in)
{
    nrows = 24; //client->nMuxRows();
    ncols = 8; //client->nMuxCols();

    chansocket = new zmq::socket_t(*zmqcontext, ZMQ_PUB);
    try {
        chansocket->bind(CHANSUBPORT);
        std::cout << "chansocket publisher connected" << std::endl;
    } catch (zmq::error_t) {
        delete chansocket;
        chansocket = NULL;
    }


    setWindowFlags(Qt::Window);
    setAttribute(Qt::WA_DeleteOnClose); // important!
    ui->setupUi(this);
    setWindowTitle("Matter data plots");

    // Build layout with the NUM_SPINNERS (8?) channel selection spin boxes
    QGridLayout *chanSpinnersLayout = new QGridLayout(0);
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
        box->setRange(-1, 999);
        box->setSpecialValueText("--");
        box->setValue(-1);\
        box->setPrefix("Ch ");
        box->setAlignment(Qt::AlignRight);
        spinners.push_back(box);
        selectedChannel.push_back(-1);
        connect(box, SIGNAL(valueChanged(int)), this, SLOT(channelChanged(int)));

        chanSpinnersLayout->addWidget(label, i, 0);
        chanSpinnersLayout->addWidget(box, i, 1);
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
    analysisMenuActionGroup.addAction(ui->actionBaseline);
    analysisMenuActionGroup.addAction(ui->actionPulse_max);
    analysisMenuActionGroup.addAction(ui->actionPulse_RMS);
    ui->actionPulse_max->setChecked(true);
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

    // Set up the plot object (which already exists)
    QCustomPlot *pl = ui->plot;
    pl->setNotAntialiasedElements(QCP::aePlottables);
    pl->setInteraction(QCP::iRangeDrag, true);
    pl->setInteraction(QCP::iRangeZoom, true);

    // Set up x axes
    pl->xAxis->setLabel("Sample number");
    connect(ui->xminBox, SIGNAL(valueChanged(double)), this, SLOT(typedXAxisMin(double)));
    connect(ui->xmaxBox, SIGNAL(valueChanged(double)), this, SLOT(typedXAxisMax(double)));
    connect(ui->xrangeBox, SIGNAL(valueChanged(double)), this, SLOT(typedXAxisRange(double)));

    // make left and bottom axes always transfer their ranges to right and top axes:
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

    // Restore relevant settings
    matterSettings = new QSettings();
    preferVisibleMinMaxRange = matterSettings->value("plots/visibleMinMaxRange",false).toBool();
    preferYaxisRawUnits = matterSettings->value("plots/yaxisRawUnits", true).toBool();

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
int plotWindow::chan2trace(int channum) {
    for (int i=0; i<NUM_TRACES; i++) {
        if (channum == selectedChannel[i])
            return i;
    }
    return -1;
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Start the thread that refreshed the plot traces
///
void plotWindow::startRefresh(void) {
    const int PLOTPERIOD_MSEC=750;
    refreshPlotsThread = new refreshPlots(PLOTPERIOD_MSEC);

    // This connection is how the traces in the plots will be updated
    connect(refreshPlotsThread, SIGNAL(newDataToPlot(int, const QVector<double> &)),
            this, SLOT(newPlotTrace(int, const QVector<double> &)));
    connect(refreshPlotsThread, SIGNAL(newDataToPlot(int, const QVector<double> &, const QVector<double> &)),
            this, SLOT(newPlotTrace(int, const QVector<double> &, const QVector<double> &)));
    connect(refreshPlotsThread, SIGNAL(addDataToPlot(int, const QVector<double> &, const QVector<double> &)),
            this, SLOT(addPlotData(int, const QVector<double> &, const QVector<double> &)));
    connect(ui->clearDataButton, SIGNAL(clicked()),
            refreshPlotsThread, SLOT(clearHistograms()));
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Destructor
///
plotWindow::~plotWindow()
{
    delete ui;
}



///////////////////////////////////////////////////////////////////////////////
/// Reimplement closeEvent so we can be sure the refresh thread is stopped.
/// \param event  The QCloseEvent that causes this to close.
///
void plotWindow::closeEvent(QCloseEvent *event)
{
    delete refreshPlotsThread;
    refreshPlotsThread = NULL;
    QMainWindow::closeEvent(event);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Render a plot onto a given trace number.
/// \param tracenum  Which trace (color) this is.
/// \param data      The raw data vector
/// \param nsamples  The length of the raw data
///
void plotWindow::newPlotTrace(int tracenum, const uint16_t *data, int nsamples)
{
    QVector<double> tdata, ddata;
    tdata.resize(nsamples);
    ddata.resize(nsamples);
    for (int i=0; i<nsamples; i++) {
        tdata[i] = i-num_presamples;
        ddata[i] = double(data[i]);
    }
    newPlotTrace(tracenum, tdata, ddata);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Render a plot onto a given trace number.
/// \param tracenum  Which trace (color) this is.
/// \param data      The raw data vector
/// \param nsamples  The length of the raw data
///
void plotWindow::newPlotTrace(int tracenum, const uint32_t *data, int nsamples)
{
    QVector<double> ddata;
    ddata.resize(nsamples);
    for (int i=0; i<nsamples; i++)
        ddata[i] = double(data[i]);
    newPlotTrace(tracenum, ddata);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Render a plot onto a given trace number.
/// \param tracenum  Which trace (color) this is.
/// \param data      The raw data vector
/// \param nsamples  The length of the raw data
///
void plotWindow::newPlotTrace(int tracenum, const int16_t *data, int nsamples)
{
    newPlotTrace(tracenum, reinterpret_cast <const uint16_t *>(data), nsamples);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Render a plot onto a given trace number.
/// \param tracenum  Which trace (color) this is.
/// \param data      The raw data vector
/// \param nsamples  The length of the raw data
///
void plotWindow::newPlotTrace(int tracenum, const int32_t *data, int nsamples)
{
    newPlotTrace(tracenum, reinterpret_cast <const uint32_t *>(data), nsamples);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Render a plot onto a given trace number.
/// \param tracenum  Which trace (color) this is.
/// \param data      The raw data vector for the y-axis
///
void plotWindow::newPlotTrace(int tracenum, const QVector<double> &data)
{

    // The x-axis trivially plots integers 0 to N-1.
    // Make sure we have a vector long enough to do that.
    int nsamples = data.size();
    if (sampleIndex.size() < nsamples) {
        int start = sampleIndex.size();
        sampleIndex.resize(nsamples);
        for (int i=start; i<nsamples; i++)
            sampleIndex[i] = i;
    }
    newPlotTrace(tracenum, sampleIndex, data);
}


///////////////////////////////////////////////////////////////////////////////
/// \brief Render a plot onto a given trace number.
/// \param tracenum  Which trace (color) this is.
/// \param xdata     The raw data vector for the x-axis
/// \param data      The raw data vector for the y-axis
///
void plotWindow::newPlotTrace(int tracenum, const QVector<double> &xdata,
                              const QVector<double> &data)
{
    QCPGraph *graph = ui->plot->graph(tracenum);

    // Convert y and (if err-vs-FB) sometimes x data to physical units.
    if (!preferYaxisRawUnits) {
        const int N=data.size();

        QVector<double> scaled_data;
        scaled_data.resize(N);
        double phys_per_raw;
        const int cnum = selectedChannel[tracenum];

        // Scale the data by the appropriate physical/raw ratio.
        // Use Error units if (1) this channel is an error one or (2) it's an Err vs FB plot.
        if (cnum%2 == 0 || plotType == PLOTTYPE_ERRVSFB)
            phys_per_raw = phys_per_avgErr;
        else
            phys_per_raw = phys_per_rawFB;
        if (plotType == PLOTTYPE_PSD)
            phys_per_raw = phys_per_raw*phys_per_raw;
        for (int i=0; i<N; i++)
            scaled_data[i] = data[i] * phys_per_raw;

        switch (plotType) {
            case PLOTTYPE_ERRVSFB:
            {
                QVector<double> scaled_xdata;
                scaled_xdata.resize(N);
                for (int i=0; i<N; i++)
                    scaled_xdata[i] = xdata[i] * phys_per_rawFB;
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
        const int N=data.size();
        QVector<double> derivdata( data );
        for (int i=0; i<N-1; i++)
            derivdata[i] = derivdata[i+1]-derivdata[i];
        derivdata[N-1] = derivdata[N-2];
        graph->setData(xdata, derivdata);
    } else
        graph->setData(xdata, data);
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
/// \brief Render a plot onto a given trace number.
/// \param tracenum  Which trace (color) this is.
/// \param xdata     The raw data vector for the x-axis
/// \param data      The raw data vector for the y-axis
///
void plotWindow::addPlotData(int tracenum, const QVector<double> &xdata,
                              const QVector<double> &data)
{
    if (xdata.size() == 0 && data.size() == 0)
        return;
    QCPGraph *graph = ui->plot->graph(tracenum);
    graph->addData(xdata, data);
    /// \todo Will need a way to remove data after there's too much. see graph->removeDataBefore()
    rescalePlots(graph);
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Slot to be informed that the sample time has changed.
/// \param dt  The new sample time (in seconds)
///
void plotWindow::newSampleTime(double dt)
{
    if (dt*1000. == ms_per_sample)
        return;
    ms_per_sample = dt*1000.;
    updateXAxisRange(ui->plot->xAxis->range());
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Slot to be informed that the record length or pretrig length has changed.
///
/// Currently we don't use the record length, but here it is anyway.
///
/// \param nsamp     Number of samples in a record
/// \param npresamp  Number of pre-trigger samples
///
void plotWindow::newRecordLengths(int nsamp, int npresamp)
{
    if (num_presamples == npresamp)
        return;
    num_presamples = npresamp;
    num_samples = nsamp;
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
    QStringList::const_iterator pstr;
    for (spin_id = 0, pstr=splitText.constBegin();
         pstr != splitText.constEnd(); pstr++) {
        if (spin_id >= NUM_TRACES)
            break;
        bool ok;
        int i = pstr->toInt(&ok, 10);
        if (!ok)
            i = -1;
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
    quickSelectErrChan1.clear();
    quickSelectErrChan2.clear();
    quickSelectErrChan1.push_back(-1);
    quickSelectErrChan2.push_back(-1);
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
            QString text("Col %1: ch %2-%3");
            int c2 = 2*c*nrows+2*e*entries_per_label;
            int c3 = c2+2*entries_per_label-2;
            // Don't let the last channel in the list be selected from column c+1!
            if (c3 >= 2*(c+1)*nrows)
                c3 = 2*(c+1)*nrows - 2;
            quickSelectErrChan1.push_back(c2);
            quickSelectErrChan2.push_back(c3);
            ui->quickFBComboBox->addItem(text.arg(c).arg(c2+1).arg(c3+1));
            ui->quickErrComboBox->addItem(text.arg(c).arg(c2).arg(c3));
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

    int c1 = quickSelectErrChan1[index];
    int c2 = quickSelectErrChan2[index];
    if (c1<0)
        return;

    for (int i=0; i<NUM_TRACES && c1<=c2; i++) {
        spinners[i]->setValue(c1);
        c1 += 2;
    }
    ui->quickFBComboBox->setCurrentIndex(0);
    updateQuickTypeText();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Use entry in quickFBComboBox to set the curve A-H values
/// \param index  Index into the comboBox entries
///
void plotWindow::updateQuickTypeFromFB(int index)
{
    if (index <= 0)
        return;

    int c1 = quickSelectErrChan1[index]+1;
    int c2 = quickSelectErrChan2[index]+1;
    if (c1<0)
        return;

    for (int i=0; i<NUM_TRACES && c1<=c2; i++) {
        spinners[i]->setValue(c1);
        c1 += 2;
    }
    ui->quickErrComboBox->setCurrentIndex(0);
    updateQuickTypeText();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Slot to update the quick-type text list of channels when the
/// selected channels are changed.
///
void plotWindow::updateQuickTypeText(void)
{
    QString text;
    for (std::vector<QSpinBox *>::iterator box=spinners.begin();
         box != spinners.end(); box++) {
        int c = (*box)->value();
        if (c>=0)
            text += QString("%1,").arg(c);
        else
            text += "-,";
    }
    text.chop(1); // remove trailing comma
    ui->quickChanEdit->setPlainText(text);
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
    QSpinBox *box = (QSpinBox *)sender();
    for (unsigned int i=0; i<spinners.size(); i++) {
        if (spinners[i] == box) {
            int oldChan = selectedChannel[i];
            selectedChannel[i] = newChan;
            refreshPlotsThread->changedChannel(i, newChan);
            QCPGraph *graph = ui->plot->graph(i);
            QVector<double> empty;
            graph->setData(empty, empty);

            // Signal to dataSubscriber
            char text[50];
            snprintf(text, 50, "add %d", newChan);
            zmq::message_t msg(text, strlen(text));
            chansocket->send(msg);
            if (oldChan >= 0) {
                // TODO: check that oldChan is not on another spinner.
                snprintf(text, 50, "rem %d", oldChan);
                zmq::message_t msg(text, strlen(text));
                chansocket->send(msg);
            }
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
    refreshPlotsThread->setIsHistogram(plotType == PLOTTYPE_HISTOGRAM);

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
        pl->yAxis->setLabel("Pulse height (arbs)");
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
        for (unsigned int i=0; i<spinners.size(); i++) {
            QSpinBox *box = spinners[i];
            int idx = box->value();
            if (idx %2 == 0) {
                box->setValue(idx+1);
                selectedChannel[i] = idx+1;
            }
            box->setRange(-1, 480-1); // TODO
//            box->setRange(-1, client->nDataStreams()-1);
            box->setSingleStep(2);
        }
        ui->quickErrComboBox->setCurrentIndex(0);
        ui->quickErrComboBox->setEnabled(false);
        updateQuickTypeText();
    } else {
        for (unsigned int i=0; i<spinners.size(); i++) {
            QSpinBox *box = spinners[i];
            box->setRange(-1, 480-1); // TODO
//            box->setRange(-1, client->nDataStreams()-1);
            box->setSingleStep(1);
        }
        ui->quickErrComboBox->setEnabled(true);
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
    if (action == ui->actionBaseline) {
        analysisType = ANALYSIS_BASELINE;
    } else if (action == ui->actionPulse_max) {
        analysisType = ANALYSIS_PULSE_MAX;
    } else if (action == ui->actionPulse_RMS) {
        analysisType = ANALYSIS_PUSLE_RMS;
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
    matterSettings->setValue("plots/yaxisRawUnits", preferYaxisRawUnits);
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
    matterSettings->setValue("plots/visibleMinMaxRange", preferVisibleMinMaxRange);
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
