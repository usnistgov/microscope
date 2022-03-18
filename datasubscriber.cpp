#include <iostream>
#include <sstream>
#include <unistd.h>
#include <assert.h>
#include <QThread>
#include <QTimer>
#include <string.h>
#include <zmq.hpp>

#include "datasubscriber.h"
#include "microscope.h"
#include "pulserecord.h"
#include "refreshplots.h"

dataSubscriber::dataSubscriber(plotWindow *w, zmq::context_t *zin, std::string tcpsourcein) :
    window(w),
    plotManager(w->refreshPlotsThread),
    zmqcontext(zin),
    subscriber(nullptr),
    tcpdatasource(tcpsourcein),
    sampletime(1.0)
{
    myThread = new QThread;
    moveToThread(myThread);

    // Do the same to the thread, except deleteLater happens when THREAD (not this) is finished.
    connect(myThread, SIGNAL(started()), this, SLOT(process()));

    connect(this, SIGNAL(failed()), w, SLOT(close()));
    connect(this, SIGNAL(finished()), myThread, SLOT(quit()));
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    connect(myThread, SIGNAL(finished()), myThread, SLOT(deleteLater()));

    connect(this, SIGNAL(newSampleTime(double)), window, SLOT(newSampleTime(double)));
    connect(this, SIGNAL(newSampleTime(double)), plotManager, SLOT(newSampleTime(double)));

    connect(this, SIGNAL(newDataToPlot(int, pulseRecord *)),
            plotManager, SLOT(receiveNewData(int, pulseRecord *)));

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
    if (subscriber == nullptr)
        return;
    uint16_t filternumber = static_cast<uint16_t>(channum);
    const char *filter = reinterpret_cast<char *>(&filternumber);
    #if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4, 7, 0)
    subscriber->set(zmq::sockopt::subscribe, filter);
    #else
    subscriber->setsockopt(ZMQ_SUBSCRIBE, filter, sizeof(filternumber));
    #endif
//    std::cout << "Subscribed chan " << channum << std::endl;
}

void dataSubscriber::unsubscribeChannel(int channum) {
    if (subscriber == nullptr)
        return;
    const char *filter = reinterpret_cast<char *>(&channum);
    #if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4, 7, 0)
    subscriber->set(zmq::sockopt::unsubscribe, filter);
    #else
    subscriber->setsockopt(ZMQ_UNSUBSCRIBE, filter, sizeof(filternumber));
    #endif
//    std::cout << "Unsubscribed chan " << channum << std::endl;
}

void dataSubscriber::parseChannelMessage(zmq::message_t &msg) {
    // Message will be of form: "add 35" or "rem 0".
    char txt[4];
    int channum;
    std::istringstream iss(static_cast<char*>(msg.data()));
    iss >> txt >> channum;
    if (strncmp(txt, "add", 3) == 0) {
        subscribeChannel(channum);
    } else if (strncmp(txt, "rem", 3) == 0) {
        unsubscribeChannel(channum);
    }
}


void dataSubscriber::process() {
    subscriber = new zmq::socket_t(*zmqcontext, ZMQ_SUB);
    std::cout << "Connecting to Server at " << tcpdatasource <<"...";
    try {
        subscriber->connect(tcpdatasource.c_str());
        std::cout << "done!" << std::endl;
    } catch (zmq::error_t&) {
        delete subscriber;
        subscriber = nullptr;
        std::cout << "failed!" << std::endl;
        emit failed();
        return;
    }

    zmq::socket_t *killsocket = new zmq::socket_t(*zmqcontext, ZMQ_SUB);
    try {
        killsocket->connect(KILLPORT);
        #if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4, 7, 0)
        subscriber->set(zmq::sockopt::subscribe, "Quit");
        #else
        killsocket->setsockopt(ZMQ_SUBSCRIBE, "Quit", 4);
        #endif

    } catch (zmq::error_t&) {
        delete subscriber;
        subscriber = nullptr;
        delete killsocket;
        emit failed();
        return;
    }

    zmq::socket_t *chansocket = new zmq::socket_t(*zmqcontext, ZMQ_SUB);
    try {
        chansocket->connect(CHANSUBPORT);
        #if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4, 7, 0)
        subscriber->set(zmq::sockopt::subscribe, "");
        #else
        killsocket->setsockopt(ZMQ_SUBSCRIBE, "", 4);
        #endif
    } catch (zmq::error_t&) {
        delete subscriber;
        subscriber = nullptr;
        delete killsocket;
        delete chansocket;
        emit failed();
        return;
    }

    const size_t NPOLLITEMS=3;
    zmq::pollitem_t pollitems[NPOLLITEMS] = {
        {static_cast<void *>(*killsocket), 0, ZMQ_POLLIN, 0},
        {static_cast<void *>(*chansocket), 0, ZMQ_POLLIN, 0},
        {static_cast<void *>(*subscriber), 0, ZMQ_POLLIN, 0},
    };

    zmq::message_t update, header, pulsedata;
    while (true) {
        zmq::poll(&pollitems[0], NPOLLITEMS);

        if (pollitems[0].revents & ZMQ_POLLIN) {
            // killsocket received a message. Any message there means DIE.
            #if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4, 3, 1)
            killsocket->recv(update);
            #else
            killsocket->recv(&update);
            #endif
            break;
        }
        if (pollitems[1].revents & ZMQ_POLLIN) {
            // chansocket received a message.
            #if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4, 3, 1)
            chansocket->recv(update);
            #else
            chansocket->recv(&update);
            #endif
            parseChannelMessage(update);
            continue;
        }
        if (!(pollitems[2].revents & ZMQ_POLLIN)) {
            std::cerr << "Poll timed out, but I don't know why" << std::endl;
            continue;
        }

        // Receive a 2-part message
        #if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4, 3, 1)
        subscriber->recv(header);
        #else
        subscriber->recv(&header);
        #endif
        if (!header.more()) {
            std::cerr << "Received an unexpected 1-part message" << std::endl;
            continue;
        }
        #if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4, 3, 1)
        subscriber->recv(pulsedata);
        #else
        subscriber->recv(&pulsedata);
        #endif

        pulseRecord *pr = new pulseRecord(header, pulsedata);
        // std::cout << "Received message of size " << header.size() << "+" << pulsedata.size();
        // std::cout << " for chan " << pr->channum << " with " << pr->nsamples << "/"
        //           << pr->presamples <<" samples/presamples and "
        //           << pr->wordsize <<"-byte words: [";
        // std::cout << pr->data[0] <<", " << pr->data[1] << "... "
        //           << pr->data[pr->nsamples-1] <<"] dT="<< pr->sampletime << std::endl;
        int tracenum = window->streamnum2trace(pr->channum);
        if (tracenum >= 0) {
            if (!approx_equal(pr->sampletime, sampletime, 1e-5)) {
                sampletime = double(pr->sampletime);
                emit newSampleTime(sampletime);
            }
            emit newDataToPlot(tracenum, pr);
        } else {
            delete pr;
//            std::cout << "trace num" << tracenum << std::endl;
        }
    }
    delete subscriber;
    subscriber = nullptr;
    delete killsocket;
    delete chansocket;
    myThread->quit();
}

void dataSubscriber::wait(unsigned long time) {
    myThread->wait(time);
}
