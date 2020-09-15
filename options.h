#ifndef OPTIONS_H
#define OPTIONS_H

#include <QString>

class options {
public:
    QString appname;
    int rows;
    int cols;
    bool tdm;
    bool indexing;
    bool failed;

    options();
    bool readChanGroups();
};

options *processOptions(int argc, char *argv[]);
void usage();

#endif // OPTIONS_H
