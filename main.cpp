#include <QApplication>
#include <QMetaType>
#include <QObject>
#include <QVector>
#include <vector>
#include <iostream>
#include <unistd.h> // for usleep

#include "plotwindow.h"
#include "datasubscriber.h"
#include "microscope.h"
#include "options.h"


int main(int argc, char *argv[])
{
    // The OrganizationName and ApplicationName are used to identify the
    // QSettings file, so that they can be reached from anywhere.
    QCoreApplication::setOrganizationName("NIST Quantum Sensors");
    QCoreApplication::setApplicationName("Microscope");

    options *Opt = processOptions(argc, argv);
    if (Opt->failed) {
        usage();
        return 1;
    }
    argc -= optind;
    argv += optind;
    QApplication a(argc, argv);
    qRegisterMetaType<QVector<double> >("QVector<double>");

    std::string tcpdataport = "tcp://localhost:5502";
    QStringList args = a.arguments();
    for (int i=0; i<args.length(); i++) {
        if (args[i].startsWith("tcp:")) {
            tcpdataport = args[i].toStdString();
        }
    }

    zmq::context_t zmqcontext;
    plotWindow *w = new plotWindow(&zmqcontext, Opt);
    dataSubscriber *sub = new dataSubscriber(w, &zmqcontext, tcpdataport);

    zmq::socket_t *killsocket = new zmq::socket_t(zmqcontext, ZMQ_PUB);
    try {
       killsocket->bind(KILLPORT);
    } catch (zmq::error_t&) {
        delete killsocket;
        killsocket = nullptr;
        std::cerr << "Could not bind a socket to " << KILLPORT << std::endl;
        return 0;
    }

    // Start the main event loop, and when it returns, clean up.
    w->show();
    int app_return_val = a.exec();

    const char quit[] = "Quit";
    // The following will eliminate a certain deprecation message in zmqpp >= version 4.3.1.
    // But it won't compile at (for example) version 4.1.1, hence the #if/#else/#endif.
    #if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4, 3, 1)
    zmq::const_buffer Qmsg = zmq::const_buffer(quit, strlen(quit));
    killsocket->send(Qmsg);
    #else
    killsocket->send(quit, strlen(quit));
    #endif

    sub->wait(1000);
    delete sub;
    delete killsocket;

    return app_return_val;
}
