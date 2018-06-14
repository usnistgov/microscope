#include "plotwindow.h"
#include "datasubscriber.h"
#include "microscope.h"

#include <QApplication>
#include <QMetaType>
#include <QObject>
#include <QVector>
#include <vector>
#include <iostream>
#include <unistd.h> // for usleep


int main(int argc, char *argv[])
{

    QApplication a(argc, argv);
    qRegisterMetaType<QVector<double> >("QVector<double>");

    // The OrganizationName and ApplicationName are used to identify the
    // QSettings file, so that they can be reached from anywhere.
    QCoreApplication::setOrganizationName("NIST Quantum Sensors");
    QCoreApplication::setApplicationName("Microscope");

    // You can override the ApplicationName with command-line argument of the
    // exact form -name=MatterB (or -name=whatever). Not the most flexible idea
    // you can imagine, but it's better than nothing.
    std::string tcpdataport = "tcp://localhost:5502";
    QStringList args = a.arguments();
    for (int i=1; i<args.length(); i++) {
        if (args[i].startsWith("-name=")) {
            QString name(args[i]);
            name = name.remove("-name=");
            QCoreApplication::setApplicationName(name);
        } else if (args[i].startsWith("tcp:")) {
            tcpdataport = args[i].toStdString();
        }
    }
    std::cout << "Application name for QSettings purposes: " << QCoreApplication::applicationName().toStdString() << std::endl;
    zmq::context_t zmqcontext;

    plotWindow *w = new plotWindow(&zmqcontext);
    dataSubscriber *sub = new dataSubscriber(w, &zmqcontext, tcpdataport);

    zmq::socket_t *killsocket = new zmq::socket_t(zmqcontext, ZMQ_PUB);
    try {
       killsocket->bind(KILLPORT);
    } catch (zmq::error_t) {
        delete killsocket;
        killsocket = NULL;
        std::cerr << "Could not bind a socket to " << KILLPORT << std::endl;
        return 0;
    }

    // Start the main event loop, and when it returns, clean up.
    w->show();
    int app_return_val = a.exec();

    const char quit[] = "Quit";
    killsocket->send(quit, strlen(quit));

    sub->wait(1000);
    delete sub;
    delete killsocket;

    return app_return_val;
}
