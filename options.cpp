#include "options.h"
#include <iostream>
#include <getopt.h>

options::options() :
    appname("Microscope"),
    rows(0),
    cols(0),
    tdm(true),
    indexing(false),
    failed(false)
{
}

bool options::readChanGroups() {
    return false;
}

options *processOptions(int argc, char *argv[])
{
    options *Opt = new(options);

    static struct option longopts[] = {
        { "appname", required_argument, nullptr, 'a'},
        { "rows", optional_argument, nullptr, 'r'},
        { "columns", optional_argument, nullptr, 'c'},
        { "indexing", no_argument, nullptr, 'i'},
        { "no-error-channel", no_argument, nullptr, 'n'},
        { "help", no_argument, nullptr, 'h'},
        { nullptr, 0, nullptr, 0 }
    };

    int ch;
    QString name;
    while ((ch = getopt_long(argc, argv, "hna:ir:c:", longopts, nullptr)) != -1) {
        switch (ch) {
        case 'h':
            Opt->failed = true;
            return Opt;
        case 'n':
            Opt->tdm = false;
            break;
        case 'a':
            Opt->appname = QString(optarg);
            break;
        case 'i':
            Opt->indexing = true;
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

    // We can using indexing, read the config file to learn the channel groups, or set rows+cols.
    if (Opt->indexing) {
        return Opt;
    }
    if (Opt->rows==0 || Opt->cols==0) {
         if (!Opt->readChanGroups()) {
             std::cerr << "Could not read the channel file $HOME/.dastard/channels.json\n";
             std::cerr << "Therefore, you must set row+column counts with -rNR -cNC, or use indexing with -i." << std::endl;
             Opt->failed = true;
         }
    }

    return Opt;
}


void usage() {
    std::cerr << "Usage: microscope [options] [data records host]\n"
              << "Default host is  tcp://localhost:5502\n"
              << "Options include:\n"
              << "     -h, --help              Print this help message\n"
              << "     -i, --indexing          Number channels from 0, instead of using actual channel numbers\n"
              << "     -r, --rows              Number channels assuming this many rows\n"
              << "     -c, --columns           Number channels assuming this many columns\n"
              << "     -n, --no-error-channel  This is a non-TDM system and has no error channels\n"
              << "     -a, --appname AppName   Change the app name on the window title bar\n" << std::endl;
}


