#include "plotwindow.h"
#include "datasubscriber.h"

#include <QApplication>
#include <QMetaType>
#include <QObject>
#include <QVector>
#include <vector>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication::setGraphicsSystem("raster");

    QApplication a(argc, argv);
    qRegisterMetaType<QVector<double> >("QVector<double>");

    // The OrganizationName and ApplicationName are used to identify the
    // QSettings file, so that they can be reached from anywhere.
    QCoreApplication::setOrganizationName("NIST Quantum Sensors");
    QCoreApplication::setApplicationName("Microscope");

    // You can override the ApplicationName with command-line argument of the
    // exact form -name=MatterB (or -name=whatever). Not the most flexible idea
    // you can imagine, but it's better than nothing.
    QStringList args = a.arguments();
    for (int i=1; i<args.length(); i++) {
        if (args[i].startsWith("-name=")) {
            QString name(args[i]);
            name = name.remove("-name=");
            QCoreApplication::setApplicationName(name);
        }
        std::cout << "Application name for QSettings purposes: " << QCoreApplication::applicationName().toStdString() << std::endl;
    }

    plotWindow *w = new plotWindow();
    w->show();
    dataSubscriber *sub = new dataSubscriber();

    // Start the main event loop, and when it returns, clean up.
    int app_return_val = a.exec();
    delete w;
    return app_return_val;
}
