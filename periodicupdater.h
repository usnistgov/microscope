#ifndef PERIODICUPDATER_H
#define PERIODICUPDATER_H

///////////////////////////////////////////////////////////////////////////////
/// \file periodicupdater.h
/// Declares the periodicUpdater object, which you subclass for running
/// a work task repeatedly at regular intervals.
///

#include <QObject>

class QThread;
class QTimer;


///////////////////////////////////////////////////////////////////////////////
/// Object to run in a private thread, waking up periodically to refresh
/// any GUI objects that need periodic, manual, refreshing.
/// This is an abstract base class for specific implementations.
///
class periodicUpdater : public QObject
{
    Q_OBJECT

public:
    periodicUpdater(int msec);
    virtual ~periodicUpdater();

    void setRefreshTime(int rt);

    /// \return How many times the workQuantum() has been called
    int timesCalled(void) const {return calls;}

signals:
    /// Signal emitted when this object completes (i.e., its destructor is called)
    void finished(void);

public slots:
    /// Slot to call when it's time to terminate this thread.
    void terminate(void) {emit finished();}

    /// Slot to call when the timer expires and runs another quantum of work.
    void timerExpired(void) {calls++; workQuantum();}

protected:
    virtual void workQuantum(void)=0; ///< Subclasses implement this as the work unit.

private:
    int calls;             ///< Number of refreshes that have occurred so far.
    QThread *myThread;     ///< The QThread in which this object's work is performed.
    QTimer *timer;         ///< A timer for repeating the workQuantum call.
};

#endif // PERIODICUPDATER_H
