////////////////////////////////////////////////////////////////////////////////////
/// \file channelselectwidgets.cpp
/// Define the classes for widgets that controll which channels do and don't
/// do things, such as streaming to this client or turning analysis on/off.

#include <iostream>
#include <QCheckBox>
#include <QSettings>
#include <QString>

#include "client.h"
#include "channelselectwidgets.h"



////////////////////////////////////////////////////////////////////////////////////
/// \brief Constructs simply by calling the base QCheckBox constructor
/// \param name    Check box's name (printed on screen)
/// \param parent  Parent QWidget, for memory management
///

masterCheckBox::masterCheckBox(const QString & name, QWidget *parent) :
    QCheckBox(name, parent) {
    checkBoxGroup.clear();
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Destructor does not have any duties.
///

masterCheckBox::~masterCheckBox() {
    ;
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Add a check box to this one's "group", i.e. the boxes it manages.
///
/// Must connect the signals between this master and its group members.
///
/// \param b   The QCheckBox to add to the group.
///

void masterCheckBox::addBoxToGroup(QCheckBox *b) {
    checkBoxGroup.push_back(b);

    // This lets the master control the group.
    connect(this, SIGNAL(clicked(bool)), b, SLOT(setChecked(bool)));
    // The lets the group control the master.
    connect(b, SIGNAL(stateChanged(int)), this, SLOT(memberChangedState()));
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Called when setChecked() slot was used. This allows a masterCheckBox
/// to call setChecked on its group recursively.

void masterCheckBox::checkStateSet() {
    // This callback happens when this box's master changes its state. Thus make
    // sure to get out of the Qt::PartiallyChecked state. Then match all of our
    // group's state with our own.

    Qt::CheckState state = checkState();
    if (state == Qt::PartiallyChecked)
        state = isChecked() ? Qt::Checked  : Qt::Unchecked;
    for (std::vector<QCheckBox *>::const_iterator it=checkBoxGroup.begin();
         it != checkBoxGroup.end(); it++) {
        (*it)->setCheckState(state);
    }
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Slot used when any of the member boxes had a state change.
///

void masterCheckBox::memberChangedState( ) {
    const Qt::CheckState state0 = checkBoxGroup[0]->checkState();
    for (std::vector<QCheckBox *>::const_iterator it=checkBoxGroup.begin();
         it != checkBoxGroup.end(); it++) {
        if (state0 != (*it)->checkState()) {
            setCheckState( Qt::PartiallyChecked );
            return;
        }
    }
    setCheckState(state0);
}

////////////////////////////////////////////////////////////////////////////////////
// End of class masterCheckBox
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
/// \brief Constructs simply by calling the base QCheckBox constructor
/// \param name     Check box's name (printed on screen)
/// \param idnumber This box's ID code, as emitted in its stateChanged(int, int) signal.
/// \param parent   Parent QWidget, for memory management
///

numberedCheckBox::numberedCheckBox(const QString & name, int idnumber, QWidget *parent) :
    QCheckBox(name, parent),
    boxID(idnumber)
{
    connect(this, SIGNAL(stateChanged(int)), this, SLOT(emitStateChanged(int)));
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Destructor does not have any duties.
///

numberedCheckBox::~numberedCheckBox() {
    ;
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Slot to emit a new signal, this one containing the box's unique ID.
/// \param state  Box's new Qt::CheckState.
///
void numberedCheckBox::emitStateChanged(int state)
{
    emit stateChanged(state, boxID);
}

////////////////////////////////////////////////////////////////////////////////////
// End of class numberedCheckBox
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
/// \brief Constructor to build up all the per-channel, per-row, and per-column
/// checkboxes that control some behavior.  The derived class constructors
/// handle the details.
///
/// \param nrows_in   How many rows of channels are offered by the connected server.
/// \param ncols_in   How many columns of channels are offered by the connected server.
/// \param client_in  The connected Client object.
/// \param settingsName_in  The name of the array in QSettings for storing the check state
/// \param parent     The parent Qt widget (for memory management).
///

channelSelectionWidget::channelSelectionWidget(int nrows_in, int ncols_in,
                                 xdaq::Client *client_in, const char *settingsName_in,
                                               QWidget *parent) :
    QWidget(parent),  nrows(nrows_in), ncols(ncols_in),
    grid(this), client(client_in),
    settingsName(settingsName_in),
    numCheckedBoxes(0)
{
    ;
}



/////////////////////////////////////////////////////////////////////////////////
/// \brief Trivial destructor.
///
channelSelectionWidget::~channelSelectionWidget() {
    check_boxes.clear(); // Qt object tree will delete what they point to.
    col_controls.clear();
    row_controls.clear();
}



/////////////////////////////////////////////////////////////////////////////////
/// \brief Slot to notify that a channel Trigger rate box was clicked
/// \param chan  Channel number
///
void channelSelectionWidget::channelBoxClicked(int chan)
{
    if (chan<0 || chan >= int(check_boxes.size()))
        return;
    check_boxes[chan]->toggle();
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Restore the GUI state based on saved settings
/// \param settings  The QSettings object with stored state
///
void channelSelectionWidget::restoreGuiSettings(QSettings *settings)
{
    settings->beginReadArray(settingsName);
    for (unsigned int i=0; i<check_boxes.size(); i++) {
        settings->setArrayIndex(i);
        const bool value = settings->value("on", false).toBool();
        check_boxes[i]->setChecked(value);
    }
    settings->endArray();
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Save the GUI state as settings
/// \param settings  The QSettings object where we store state
///
void channelSelectionWidget::storeGuiSettings(QSettings *settings)
{
    unsigned int num_boxes = check_boxes.size();
    settings->beginWriteArray(settingsName, num_boxes);
    for (unsigned int i=0; i<num_boxes; i++) {
        const bool value = check_boxes[i]->isChecked();
        settings->setArrayIndex(i);
        settings->setValue("on", value);
    }
    settings->endArray();
}

////////////////////////////////////////////////////////////////////////////////////
// End of class channelSelectionWidget
////////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////////
/// \brief Constructor to build up all the per-channel, per-row, and per-column
/// checkboxes that control streaming behavior.
///
/// Builds an Nrow * Ncol array of per-channel check boxes, which are mastered
/// by per-column and per-row check boxes. The per-column ones are themselves
/// mastered by an all-error and an all-FB box. Communication between masters and
/// their group is handled by the signal/slots defined in the masterCheckBox class.
///
/// \param nrows_in   How many rows of channels are offered by the connected server.
/// \param ncols_in   How many columns of channels are offered by the connected server.
/// \param client_in  The connected Client object.
/// \param parent     The parent Qt widget (for memory management).
///
streamSelectTab::streamSelectTab(int nrows_in, int ncols_in,
                                 xdaq::Client *client_in, QWidget *parent) :
    channelSelectionWidget(nrows_in, ncols_in, client_in, "streaming", parent)
{
    all_err = new masterCheckBox("All Error", this);
    all_fb = new masterCheckBox("All FB", this);
    all_err->setToolTip("Control all error channels");
    all_fb->setToolTip("Control all feedback channels");
    const int VERT_SPAN=1, HORIZ_SPAN=2;
    grid.addWidget(all_err, 0, 2, VERT_SPAN, HORIZ_SPAN, Qt::AlignCenter);
    grid.addWidget(all_fb, 0, 0, VERT_SPAN, HORIZ_SPAN, Qt::AlignCenter);

    connect(this, SIGNAL(newNumberStreamingChannels(int)),
            parent, SLOT(newNumStreamingChannels(int)));

    // Make the per-column check boxes
    // Error channels first, then feedback.
    for (int c=0; c<ncols*2; c+=2) {
        QString name = QString("C%1 Err").arg(c/2);
        masterCheckBox *colcheck = new masterCheckBox(name);
        colcheck->setToolTip(QString("Control error channels on Col %1").arg(c/2));
        col_controls.push_back(colcheck);
        all_err->addBoxToGroup(colcheck);
        grid.addWidget(colcheck, 1, c+1);
    }
    for (int c=1; c<ncols*2; c+=2) {
        QString name = QString("C%1 FB").arg(c/2);
        masterCheckBox *colcheck = new masterCheckBox(name);
        colcheck->setToolTip(QString("Control feedback channels on Col %1").arg(c/2));
        col_controls.push_back(colcheck);
        all_fb->addBoxToGroup(colcheck);
        grid.addWidget(colcheck, 1, c+1);
    }

    // Make the per-row check boxes
    for (int r=0; r<nrows; r++) {
        QString name = QString("Row %1").arg(r);
        masterCheckBox *rowcheck = new masterCheckBox(name);
        rowcheck->setToolTip(QString("Control all channels on Row %1").arg(r));
        row_controls.push_back(rowcheck);
        grid.addWidget(rowcheck, r+2, 0);
    }

    // Make the per-channel check boxes
    for (int c=0; c<ncols; c++) {
        for (int r=0; r<nrows; r++) {
            const int errcnum=2*(r+c*nrows);
            QString name = QString("ch %1").arg(errcnum);
            numberedCheckBox *cb = new numberedCheckBox(name, errcnum, this);
            row_controls[r]->addBoxToGroup(cb);
            col_controls[c]->addBoxToGroup(cb);
            cb->setChecked(client->streamDataFlag(errcnum));
            connect(cb, SIGNAL(stateChanged(int,int)), this, SLOT(oneChanNewState(int,int)));
            check_boxes.push_back(cb);
            grid.addWidget(cb, r+2, c*2+1);

            const int fbcnum = errcnum+1;
            name = QString("ch %1").arg(fbcnum);
            cb = new numberedCheckBox(name, fbcnum, this);
            row_controls[r]->addBoxToGroup(cb);
            col_controls[c+ncols]->addBoxToGroup(cb);
            cb->setChecked(client->streamDataFlag(fbcnum));
            connect(cb, SIGNAL(stateChanged(int,int)), this, SLOT(oneChanNewState(int,int)));
            check_boxes.push_back(cb);
            grid.addWidget(cb, r+2, c*2+2);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////////
/// \brief Trivial destructor.

streamSelectTab::~streamSelectTab() {
    ;
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Slot to inform the client on one channel's new state
/// \param state    2=on, 1=partial, 0=off
/// \param channum  Client channel number
///
void streamSelectTab::oneChanNewState(int state, int channum)
{
    bool was_checked = client->streamDataFlag(channum);
    bool will_check = state>0;
    if (was_checked != will_check) {
        client->streamDataFlag(channum, will_check);
        if (will_check)
            numCheckedBoxes++;
        else
            numCheckedBoxes--;
    }
    emit newNumberStreamingChannels(numCheckedBoxes);
}



////////////////////////////////////////////////////////////////////////////////////
// End of class streamSelectTab
////////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////////
/// \brief Constructor to build up all the per-channel, per-row, and per-column
/// checkboxes that control analysis behavior.
///
/// Builds an Nrow * Ncol array of per-channel check boxes, which are mastered
/// by per-column and per-row check boxes. The per-column ones are themselves
/// mastered by an all-channel box. Communication between masters and
/// their group is handled by the signal/slots defined in the masterCheckBox class.
///
/// \param nrows_in   How many rows of channels are offered by the connected server.
/// \param ncols_in   How many columns of channels are offered by the connected server.
/// \param client_in  The connected Client object.
/// \param parent     The parent Qt widget (for memory management).
///
analysisSelectTab::analysisSelectTab(int nrows_in, int ncols_in,
                                 xdaq::Client *client_in, QWidget *parent) :
    channelSelectionWidget(nrows_in, ncols_in, client_in, "analysis", parent)
{
    all_chan = new masterCheckBox("All Chan", this);
    all_chan->setToolTip("Control all feedback channels");
    const int VERT_SPAN=1, HORIZ_SPAN=2;
    grid.addWidget(all_chan, 0, 2, VERT_SPAN, HORIZ_SPAN, Qt::AlignCenter);

    connect(this, SIGNAL(newNumberAnalysisChannels(int)),
            parent, SLOT(newNumAnalysisChannels(int)));

    // Make the per-column check boxes
    for (int c=0; c<ncols; c++) {
        QString name = QString("Col %1").arg(c);
        masterCheckBox *colcheck = new masterCheckBox(name);
        colcheck->setToolTip(QString("Control channels on Col %1").arg(c));
        col_controls.push_back(colcheck);
        all_chan->addBoxToGroup(colcheck);
        grid.addWidget(colcheck, 1, c+1);
    }

    // Make the per-row check boxes
    for (int r=0; r<nrows; r++) {
        QString name = QString("Row %1").arg(r);
        masterCheckBox *rowcheck = new masterCheckBox(name);
        rowcheck->setToolTip(QString("Control all channels on Row %1").arg(r));
        row_controls.push_back(rowcheck);
        grid.addWidget(rowcheck, r+2, 0);
    }

    // Make the per-channel check boxes
    for (int c=0; c<ncols; c++) {
        for (int r=0; r<nrows; r++) {
            const int fbcnum = 2*(r+c*nrows)+1;
            QString name = QString("ch %1").arg(fbcnum);
            numberedCheckBox *cb = new numberedCheckBox(name, fbcnum, this);
            row_controls[r]->addBoxToGroup(cb);
            col_controls[c]->addBoxToGroup(cb);
            cb->setChecked(client->streamDataFlag(fbcnum));
            connect(cb, SIGNAL(stateChanged(int,int)), this, SLOT(oneChanNewState(int,int)));
            check_boxes.push_back(cb);
            grid.addWidget(cb, r+2, c+1);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////////
/// \brief Trivial destructor.

analysisSelectTab::~analysisSelectTab() {
    ;
}



////////////////////////////////////////////////////////////////////////////////////
/// \brief Slot to inform the client on one channel's new state
/// \param state    2=on, 1=partial, 0=off
/// \param channum  Client channel number
///
void analysisSelectTab::oneChanNewState(int state, int channum)
{
    bool was_checked = client->performAnalysisFlag(channum);
    bool will_check = state>0;
    if (was_checked != will_check) {
        client->performAnalysisFlag(channum, will_check);
        if (will_check)
            numCheckedBoxes++;
        else
            numCheckedBoxes--;
    }
    emit newNumberAnalysisChannels(numCheckedBoxes);
}



////////////////////////////////////////////////////////////////////////////////////
// End of class analysisSelectTab
////////////////////////////////////////////////////////////////////////////////////
