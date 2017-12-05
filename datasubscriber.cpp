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

    myThread->start();
}


///////////////////////////////////////////////////////////////////////////////
/// Destructor. Note that myThread is smart enough to delete itself, thanks
/// to the connections made in the constructor.
///
dataSubscriber::~dataSubscriber() {
    emit finished();
}


void dataSubscriber::process() {
    zmq::context_t *zmqcontext = new zmq::context_t();
    zmq::socket_t *subscriber = new zmq::socket_t(*zmqcontext, ZMQ_SUB);
    std::string rpcport = "tcp://localhost:";
    const long long int pn = 5556; //port();
    rpcport += std::to_string(pn);
    std::cout << "Connecting to Server at " << rpcport <<"...";
    try {
        subscriber->connect(rpcport.c_str());
        const char *filter = "";
        subscriber->setsockopt(ZMQ_SUBSCRIBE, filter, strlen (filter));
        std::cout << "done!" << std::endl;
    } catch (zmq::error_t) {
        delete subscriber;
        subscriber = NULL;
        std::cout << "failed!" << std::endl;
        return;
    }

    while (true) {
        zmq::message_t update;
        subscriber->recv(&update);
        std::cout << "Received message of size " << update.size();

        uint32_t * lptr = reinterpret_cast<uint32_t *>(update.data());
        std::cout << " for chan " << lptr[0] << " with " << lptr[1] <<" presamples and "
                  << lptr[2] <<"-byte words: [";
        int N = (update.size()-3*sizeof(uint32_t))/lptr[2];
        uint16_t * sptr = reinterpret_cast<uint16_t *>(&lptr[3]);
        std::cout << sptr[0] <<", " << sptr[1] << "... " << sptr[N-1] <<"]"<< std::endl;

        window->newPlotTrace(lptr[0], sptr, N);
    }
}
