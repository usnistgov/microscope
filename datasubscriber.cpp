#include <iostream>
#include <sstream>
#include <unistd.h>
#include <QThread>
#include <QTimer>
#include <zmq.hpp>

#include "microscope.h"
#include "datasubscriber.h"
#include "refreshplots.h"

dataSubscriber::dataSubscriber(plotWindow *w, zmq::context_t *zin) :
    window(w),
    plotManager(w->refreshPlotsThread),
    zmqcontext(zin),
    sampletime(1.0),
    nsamples(0),
    presamples(0)
{
    myThread = new QThread;
    moveToThread(myThread);

    // Do the same to the thread, except deleteLater happens when THREAD (not this) is finished.
    connect(myThread, SIGNAL(started()), this, SLOT(process()));
    connect(this, SIGNAL(finished()), myThread, SLOT(quit()));
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    connect(myThread, SIGNAL(finished()), myThread, SLOT(deleteLater()));

    connect(this, SIGNAL(newSampleTime(double)), window, SLOT(newSampleTime(double)));
    connect(this, SIGNAL(newRecordLengths(int,int)), window, SLOT(newRecordLengths(int,int)));

    myThread->start();
}


///////////////////////////////////////////////////////////////////////////////
/// Destructor. Note that myThread is smart enough to delete itself, thanks
/// to the connections made in the constructor.
///
dataSubscriber::~dataSubscriber() {
    delete chansocket;
    delete killsocket;
    delete subscriber;
    emit finished();
}


void dataSubscriber::subscribeChannel(int channum) {
    if (subscriber == 0)
        return;
    const char *filter = reinterpret_cast<char *>(&channum);
    subscriber->setsockopt(ZMQ_SUBSCRIBE, filter, sizeof(int));
//    std::cout << "Subscribed chan " << channum << std::endl;
}

void dataSubscriber::unsubscribeChannel(int channum) {
    if (subscriber == 0)
        return;
    const char *filter = reinterpret_cast<char *>(&channum);
    subscriber->setsockopt(ZMQ_UNSUBSCRIBE, filter, sizeof(int));
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

    killsocket = new zmq::socket_t(*zmqcontext, ZMQ_SUB);
    try {
        killsocket->connect(KILLPORT);
        killsocket->setsockopt(ZMQ_SUBSCRIBE, "Quit", 4);
        std::cout << "killsocket subscriber connected" << std::endl;
    } catch (zmq::error_t) {
        delete killsocket;
        killsocket = NULL;
        return;
    }

    chansocket = new zmq::socket_t(*zmqcontext, ZMQ_SUB);
    try {
        chansocket->connect(CHANSUBPORT);
        chansocket->setsockopt(ZMQ_SUBSCRIBE, "", 0);
        std::cout << "chansocket subscriber connected" << std::endl;
    } catch (zmq::error_t) {
        delete chansocket;
        chansocket = NULL;
        return;
    }

    const size_t NPOLLITEMS=3;
    zmq::pollitem_t pollitems[NPOLLITEMS] = {
        {static_cast<void *>(*killsocket), 0, ZMQ_POLLIN, 0},
        {static_cast<void *>(*chansocket), 0, ZMQ_POLLIN, 0},
        {static_cast<void *>(*subscriber), 0, ZMQ_POLLIN, 0},
    };

    zmq::message_t update;
    while (true) {
        zmq::poll(&pollitems[0], NPOLLITEMS, -1);

        if (pollitems[0].revents & ZMQ_POLLIN) {
            // killsocket received a message. Any message there means DIE.
            killsocket->recv(&update);
            break;
        }
        if (pollitems[1].revents & ZMQ_POLLIN) {
            // chansocket received a message.
            chansocket->recv(&update);
            parseChannelMessage(update);
            continue;
        }
        if (!(pollitems[2].revents & ZMQ_POLLIN)) {
            std::cerr << "Poll timed out, but I don't know why" << std::endl;
            continue;
        }

        subscriber->recv(&update);
        pulseRecord *pr = new pulseRecord(update);

        std::cout << "Received message of size " << update.size();
        std::cout << " for chan " << pr->channum << " with " << pr->nsamples << "/"
                  << pr->presamples <<" samples/presamples and "
                  << pr->wordsize <<"-byte words: [";
        std::cout << pr->data[0] <<", " << pr->data[1] << "... "
                  << pr->data[pr->nsamples-1] <<"]"<< pr->sampletime << std::endl;
        int tracenum = window->chan2trace(pr->channum);
        if (tracenum >= 0) {
            if (pr->presamples != presamples || pr->nsamples != nsamples) {
                presamples = pr->presamples;
                nsamples = pr->nsamples;
                emit newRecordLengths(nsamples, presamples);
            }
            if (pr->sampletime != sampletime) {
                sampletime = pr->sampletime;
                emit newSampleTime(sampletime);
            }
            window->newPlotTrace(tracenum, pr->data, pr->nsamples);
        }
        delete pr;
    }

    myThread->quit();
}

void dataSubscriber::wait(unsigned long time) {
    myThread->wait(time);
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
    sampletime = *reinterpret_cast<const float *>(&lptr[3]);
    voltsperarb = *reinterpret_cast<const float *>(&lptr[4]);
    const size_t prefix_size = 3*sizeof(uint32_t) + 2*sizeof(float);
    nsamples = (message.size()-prefix_size)/wordsize;
    data = reinterpret_cast<const uint16_t *>(&lptr[5]);
//    data = new uint16_t[nsamples];
//    memcpy(data, lptr+prefix_size, message.size()-prefix_size);
}

pulseRecord::~pulseRecord() {
//    delete[] data;
}
