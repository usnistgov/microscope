#ifndef CHANNELSELECTWIDGETS_H
#define CHANNELSELECTWIDGETS_H

////////////////////////////////////////////////////////////////////////////////
/// \file channelselectwidgets.h
/// Declare the streamSelectTab and analysisSelectTab classes,
/// which controll whether channels do or don't stream to this client or do
/// pulse analysis.

#include <vector>
#include <QCheckBox>
#include <QGridLayout>
#include <QString>
#include <QWidget>

namespace xdaq {
class Client;
}
class QSettings;


////////////////////////////////////////////////////////////////////////////////
/// \brief A GUI checkbox that controls and is controlled by several others
///
///

class masterCheckBox : public QCheckBox {
    Q_OBJECT

public:
    explicit masterCheckBox(const QString &name, QWidget *parent = 0);
    virtual ~masterCheckBox();

    void addBoxToGroup(QCheckBox *b);

    /// \return The number of boxes in this master's group.
    int boxGroupSize (void) const {return checkBoxGroup.size();}

private:
    virtual void checkStateSet(void);

    /// Container for the check boxes that this one masters
    std::vector<QCheckBox *> checkBoxGroup;

private slots:
    void memberChangedState();
};



////////////////////////////////////////////////////////////////////////////////
/// \brief A GUI checkbox that sends an signal with an ID number
///
///

class numberedCheckBox : public QCheckBox {
    Q_OBJECT

public:
    explicit numberedCheckBox(const QString &name, int idnumber, QWidget *parent = 0);
    virtual ~numberedCheckBox();

signals:
    void stateChanged(int state, int id); ///< Signal new state AND box's ID code.

private:
    int boxID;    ///< The ID code this box emits in its stateChanged(int,int) signal.

private slots:
    void emitStateChanged(int state);
};



////////////////////////////////////////////////////////////////////////////////
/// \brief The GUI widget to allow user to turn on/off some aspect of various data
/// channels.
///
class channelSelectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit channelSelectionWidget(int nrows_in, int ncols_in,
                                    xdaq::Client *client_in,
                                    const char *settingsName_in,
                                    QWidget *parent = 0);
    virtual ~channelSelectionWidget();
    void restoreGuiSettings(QSettings *settings);
    void storeGuiSettings(QSettings *settings);

public slots:
    void channelBoxClicked(int chan);

protected slots:
    /// \brief Slot to inform the client on one channel's new state
    virtual void oneChanNewState(int, int)=0;

protected:
    const int nrows;          ///< How many rows are multiplexed
    const int ncols;          ///< How many columns are multiplexed
    std::vector<numberedCheckBox *> check_boxes; ///< All per-channel check boxes
    std::vector<masterCheckBox *> col_controls;  ///< The per-column check boxes
    std::vector<masterCheckBox *> row_controls;  ///< The per-row check boxes

    QGridLayout grid;         ///< The layout for all these check boxes
    xdaq::Client *client;     ///< The underlying client object.
    const QString settingsName; ///< Name used for storing values in QSettings
    int numCheckedBoxes;      ///< How many boxes are checked
};



////////////////////////////////////////////////////////////////////////////////
/// \brief The GUI widget to allow user to turn on/off various data
/// channels for streaming from server to us.
///
class streamSelectTab : public channelSelectionWidget
{
    Q_OBJECT

public:
    explicit streamSelectTab(
            int nrows_in, int ncols_in, xdaq::Client *client_in,
            QWidget *parent = 0);
    virtual ~streamSelectTab();

signals:
    void newNumberStreamingChannels(int); ///< The number of channels being streamed has changed.

protected slots:
    virtual void oneChanNewState(int, int);

private:
    masterCheckBox *all_err;  ///< The check box to control all error channels
    masterCheckBox *all_fb;   ///< The check box to control all FB channels
};


////////////////////////////////////////////////////////////////////////////////
/// \brief The GUI widget to allow user to turn on/off various data
/// channels for analysis.
///
class analysisSelectTab : public channelSelectionWidget
{
    Q_OBJECT

public:
    explicit analysisSelectTab(
            int nrows_in, int ncols_in, xdaq::Client *client_in,
            QWidget *parent = 0);
    virtual ~analysisSelectTab();

signals:
    void newNumberAnalysisChannels(int); ///< The number of channels being analyzed has changed.

protected slots:
    virtual void oneChanNewState(int, int);

private:
    masterCheckBox *all_chan;   ///< The check box to control all channels
};


#endif // CHANNELSELECTWIDGETS_H
