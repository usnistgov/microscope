#include <iostream>
#include <sstream>
#include <unistd.h>
#include <assert.h>
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
    connect(this, SIGNAL(newSampleTime(double)), plotManager, SLOT(newSampleTime(double)));
    connect(this, SIGNAL(newRecordLengths(int,int)), window, SLOT(newRecordLengths(int,int)));

    connect(this, SIGNAL(newDataToPlot(int, const uint16_t *, int, int)),
            plotManager, SLOT(receiveNewData(int, const uint16_t *, int, int)));

    //    connect(this, SIGNAL(newDataToPlot(int, const QVector<double>, const QVector<double>)),
//            plotManager, SLOT(receiveNewData(int,QVector<double>, const QVector<double>)));

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
    uint16_t filternumber = channum;
    const char *filter = reinterpret_cast<char *>(&filternumber);
    subscriber->setsockopt(ZMQ_SUBSCRIBE, filter, sizeof(filternumber));
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

    zmq::message_t header, update;
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

        subscriber->recv(&header);
        assert(header.more());
        subscriber->recv(&update);
        assert( !update.more());
        pulseRecord *pr = new pulseRecord(header, update);

        std::cout << "Received message of size " <<header.size() << "+" << update.size();
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
            emit newDataToPlot(tracenum, pr->data, pr->nsamples, pr->presamples);
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
pulseRecord::pulseRecord(const zmq::message_t &header, const zmq::message_t &pulserecord) {
    const char* msg = reinterpret_cast<const char *>(header.data());

    channum = *reinterpret_cast<const uint16_t *>(&msg[0]);
    char version = msg[2];
    assert (version == 0);

    // Ignore word size code for now (assert uint16)
    char wordsizecode = msg[3];
    assert (wordsizecode == 3);
    wordsize = 2;

    presamples = *reinterpret_cast<const uint32_t *>(&msg[4]);
    sampletime = *reinterpret_cast<const float *>(&msg[8]);
    voltsperarb = *reinterpret_cast<const float *>(&msg[12]);

    time_nsec = *reinterpret_cast<const uint64_t *>(&msg[16]);
    serialnumber = *reinterpret_cast<const uint64_t *>(&msg[24]);

    nsamples = pulserecord.size() / wordsize;
    data = reinterpret_cast<const uint16_t *>(pulserecord.data());

    //    data = new uint16_t[nsamples];
//    memcpy(data, lptr+prefix_size, message.size()-prefix_size);
}

pulseRecord::~pulseRecord() {
//    delete[] data;
}
