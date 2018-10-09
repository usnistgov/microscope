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
#include <getopt.h>

options::options() :
    appname("Microscope"),
    rname("rows"),
    cname("cols"),
    rows(0),
    cols(0),
    tdm(true),
    failed(false)
{
  ;
}

options *processOptions(int argc, char *argv[])
{
    options *Opt = new(options);

    static struct option longopts[] = {
        { "appname", required_argument, NULL, 'a'},
        { "rows", required_argument, NULL, 'r'},
        { "columns", required_argument, NULL, 'c'},
        { "no-error-channel", no_argument, NULL, 'n'},
        { NULL, 0, NULL, 0 }
    };

    int ch;
    QString name;
    while ((ch = getopt_long(argc, argv, "na:r:c:", longopts, 0)) != -1) {
        switch (ch) {
        case 'n':
            Opt->tdm = false;
            break;
        case 'a':
            Opt->appname = QString(optarg);
            break;
        case 'r':
            Opt->rows = atoi(optarg);
            break;
        case 'c':
            Opt->cols = atoi(optarg);
            break;
        default:
            Opt->failed = true;
        }
    }
    if (Opt->rows==0 || Opt->cols==0) {
        std::cerr << "Must set both row and column counts to 1 or more with -rNR -cNC." << std::endl;
        Opt->failed = true;
    }

    return Opt;
}


void usage() {
    std::cerr << "Usage: microscope [--no-error-channel/-n] [--appname AppName/-a AppName]\n"
                 "       -c8 -r28 tcp://localhost:5502" << std::endl;
}




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
