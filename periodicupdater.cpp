///////////////////////////////////////////////////////////////////////////////
/// \file periodicupdater.cpp
/// Defines the periodicUpdater object, which you subclass for running
/// a work task repeatedly at regular intervals.
///

#include "periodicupdater.h"

#include <iostream>
#include <unistd.h>
#include <QThread>
#include <QTimer>


///////////////////////////////////////////////////////////////////////////////
/// \brief Constructor.
///
/// Creates a QThread, moves work to that thread, and starts it.
/// \param msec  Period of refresh, in milliseconds.
///
periodicUpdater::periodicUpdater(int msec) :
    calls(0), myThread(NULL)
{
    timer = new QTimer;
    myThread = new QThread;
    moveToThread(myThread);

    // When done, we'll stop the timer and have it delete itself
    connect(this, SIGNAL(finished()), timer, SLOT(stop()));
    connect(this, SIGNAL(finished()), timer, SLOT(deleteLater()));

    // Do the same to the thread, except deleteLater happens when THREAD (not this) is finished.
    connect(this, SIGNAL(finished()), myThread, SLOT(quit()));
    connect(myThread, SIGNAL(finished()), myThread, SLOT(deleteLater()));

    connect(timer, SIGNAL(timeout()), this, SLOT(timerExpired()));
    timer->start(msec);
    myThread->start();
}



///////////////////////////////////////////////////////////////////////////////
/// Destructor. Note that myThread is smart enough to delete itself, thanks
/// to the connections made in the constructor.
///
periodicUpdater::~periodicUpdater() {
    emit finished();
}



///////////////////////////////////////////////////////////////////////////////
/// \brief Change the refresh time for this thread.
/// \param rt   New refresh time (milliseconds)
///
void periodicUpdater::setRefreshTime(int rt)
{
    timer->setInterval(rt);
}






