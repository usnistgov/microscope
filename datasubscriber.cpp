#include <iostream>
#include <unistd.h>
#include <QThread>
#include <QTimer>
#include <zmq.hpp>

#include "datasubscriber.h"

dataSubscriber::dataSubscriber()
{
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
        std::cout << "Received message of size " << update.size() << std::endl;

//        std::istringstream iss(static_cast<char*>(update.data()));
//        iss >> zipcode >> temperature >> relhumidity ;
    }
}
