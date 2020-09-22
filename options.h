#ifndef OPTIONS_H
#define OPTIONS_H

#include <QString>
#include <QVector>

struct channelGroup {
    int firstchan;
    int nchan;
};

class options {
public:
    QString appname;
    int rows;
    int cols;
    int nchan;
    int nsensors;
    bool tdm;
    bool indexing;
    bool failed;
    QVector<channelGroup> chanGroups;

    options();
    bool readChanGroups();
};

options *processOptions(int argc, char *argv[]);
void usage();

#endif // OPTIONS_H
