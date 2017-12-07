#include <iostream>
#include <sstream>
#include <unistd.h>
#include <QThread>
#include <QTimer>
#include <zmq.hpp>

#include "datasubscriber.h"
#include "plotwindow.h"

dataSubscriber::dataSubscriber(plotWindow *w)
{
    window = w;
    myThread = new QThread;
    moveToThread(myThread);

    // Do the same to the thread, except deleteLater happens when THREAD (not this) is finished.
    connect(myThread, SIGNAL(started()), this, SLOT(process()));
    connect(this, SIGNAL(finished()), myThread, SLOT(quit()));
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    connect(myThread, SIGNAL(finished()), myThread, SLOT(deleteLater()));

    // These signals would normally have to cross threads, but the receiver (this) will be blocked waiting
    // on ZMQ messages from DASTARD: the receipt would never happen if we used the default auto connections.
    // Using the direct connection means that the slot runs in the *sender's* thread, which isn't blocked.
    // I'm not sure this obey's ZMQ's thread rules, honestly. If it causes problems, then we'll need to
    // replace these signals with ZMQ messages on an inproc socket. --12/6/2017 jwf
    connect(window, SIGNAL(startPlottingChannel(int)), this, SLOT(subscribeChannel(int)), Qt::DirectConnection);
    connect(window, SIGNAL(stopPlottingChannel(int)), this, SLOT(unsubscribeChannel(int)), Qt::DirectConnection);

    myThread->start();
}


///////////////////////////////////////////////////////////////////////////////
/// Destructor. Note that myThread is smart enough to delete itself, thanks
/// to the connections made in the constructor.
///
dataSubscriber::~dataSubscriber() {
    emit finished();
}


void dataSubscriber::subscribeChannel(int channum) {
    if (subscriber == 0)
        return;
    const char *filter = reinterpret_cast<char *>(&channum);
    subscriber->setsockopt(ZMQ_SUBSCRIBE, filter, sizeof(int));
}

void dataSubscriber::unsubscribeChannel(int channum) {
    if (subscriber == 0)
        return;
    const char *filter = reinterpret_cast<char *>(&channum);
    subscriber->setsockopt(ZMQ_UNSUBSCRIBE, filter, sizeof(int));
}


void dataSubscriber::process() {
    zmq::context_t *zmqcontext = new zmq::context_t();
    subscriber = new zmq::socket_t(*zmqcontext, ZMQ_SUB);
    std::string rpcport = "tcp://localhost:";
    const long long int pn = 5556; //port();
    rpcport += std::to_string(pn);
    std::cout << "Connecting to Server at " << rpcport <<"...";
    try {
        subscriber->connect(rpcport.c_str());
        std::cout << "done!" << std::endl;
    } catch (zmq::error_t) {
        delete subscriber;
        subscriber = NULL;
        std::cout << "failed!" << std::endl;
        return;
    }

    zmq::message_t update;
    while (true) {
        subscriber->recv(&update);
        std::cout << "Received message of size " << update.size();

        pulseRecord *pr = new pulseRecord(update);
        std::cout << " for chan " << pr->channum << " with " << pr->nsamples << "/"
                  << pr->presamples <<" samples/presamples and "
                  << pr->wordsize <<"-byte words: [";
        std::cout << pr->data[0] <<", " << pr->data[1] << "... "
                  << pr->data[pr->nsamples-1] <<"]"<< std::endl;
        int tracenum = window->chan2trace(pr->channum);
        if (tracenum >= 0) {
            window->newPlotTrace(tracenum, pr->data, pr->nsamples);
        }
        delete pr;
    }
}


///////////////////////////////////////////////////////////////////////////////
/// \brief pulseRecord::pulseRecord
/// \param message
///
pulseRecord::pulseRecord(const zmq::message_t &message) {
    const uint32_t * lptr = reinterpret_cast<const uint32_t *>(message.data());
    channum = lptr[0];
    presamples = lptr[1];
    wordsize = lptr[2];
    const size_t prefix_size = 3*sizeof(uint32_t);
    nsamples = (message.size()-prefix_size)/wordsize;
    data = reinterpret_cast<const uint16_t *>(&lptr[3]);
//    data = new uint16_t[nsamples];
//    memcpy(data, lptr+prefix_size, message.size()-prefix_size);
}

pulseRecord::~pulseRecord() {
//    delete[] data;
}
