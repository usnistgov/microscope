#include <getopt.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

// Find home directory name
// From https://stackoverflow.com/questions/2910377/get-home-directory-in-linux
std::string home() {
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    return std::string(homedir);
}

#include "options.h"
#include "json11/json11.hpp"

options::options() :
    appname("Microscope"),
    rows(0),
    cols(0),
    nchan(0),
    tdm(true),
    indexing(false),
    failed(false),
    help(false)
{
}


bool options::readChanGroups() {
    std::stringstream filename;
    filename << home() << "/.dastard/channels.json";
    std::ifstream t(filename.str());
    std::stringstream buffer;
    buffer << t.rdbuf();
    std::string err;
    const auto groups = json11::Json::parse(buffer.str(), err);
    int ngroups = groups.array_items().size();
    if (ngroups == 0)
        return false;

    std::cout << "Found " << ngroups << " channel groups in " << filename.str() << std::endl;
    chanGroups.reserve(ngroups);
    nsensors = 0;
    for (int i=0; i<ngroups; i++) {
        channelGroup cg;
        cg.nchan = groups[i]["Nchan"].int_value();
        cg.firstchan = groups[i]["Firstchan"].int_value();
        chanGroups.append(cg);
        nsensors += cg.nchan;
    }
    nchan = nsensors;
    if (tdm)
        nchan *= 2;
    return chanGroups.size() > 0;
}

options *processOptions(int argc, char *argv[])
{
    options *Opt = new(options);

    static struct option longopts[] = {
        { "appname", required_argument, nullptr, 'a'},
        { "rows", optional_argument, nullptr, 'r'},
        { "columns", optional_argument, nullptr, 'c'},
        { "nsensors", optional_argument, nullptr, 'N'},
        { "indexing", no_argument, nullptr, 'i'},
        { "no-error-channel", no_argument, nullptr, 'n'},
        { "help", no_argument, nullptr, 'h'},
        { nullptr, 0, nullptr, 0 }
    };

    int ch;
    QString name;
    while ((ch = getopt_long(argc, argv, "hna:ir:c:N:", longopts, nullptr)) != -1) {
        switch (ch) {
        case 'h':
            Opt->help = true;
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
        case 'N':
            Opt->nsensors = atoi(optarg);
            break;
        default:
            Opt->failed = true;
        }
    }

    // 3 ways to number channels: we can using indexing, set rows+cols, or read the config file to learn the channel groups.
    if (Opt->indexing) {
        // Need some default values
        if (Opt->nsensors <= 0) {
            const int MAXTES = 256;
            Opt->nsensors = MAXTES;
            Opt->nchan = MAXTES;
            if (Opt->tdm)
                Opt->nchan *= 2;
            channelGroup cg;
            cg.nchan = MAXTES;
            cg.firstchan = 1;
            Opt->chanGroups.append(cg);
            std::cerr << "Warning: indexing assumes up to "<< MAXTES <<
                         " channels exist. Use -N option to raise this value." << std::endl;
        }
        return Opt;
    }

    if (Opt->rows > 0 && Opt->cols > 0) {
        // If -r and -c arguments are nonzero, treat each column as a channel group with nrows channels,
        // or double this if it's a TDM system.
        // These arguments override reading the $HOME/.dastard/channels.json file.
        int chanpercol = Opt->rows;
        if (Opt->tdm)
            chanpercol *= 2;
        Opt->nsensors = Opt->cols * Opt->rows;
        Opt->nchan = Opt->cols * chanpercol;

        int fc = 0;
        for (int i=0; i<Opt->cols; i++) {
            channelGroup cg;
            cg.nchan = chanpercol;
            cg.firstchan = fc;
            Opt->chanGroups.append(cg);
            fc += chanpercol;
        }
        return Opt;
    }

    if (Opt->rows > 0 || Opt->cols > 0) {
        std::cerr << "Command-line arguments rows=" << Opt->rows << " and cols=" << Opt->cols << " are ignored.  " <<
                     "Must set BOTH nonzero." << std::endl;
    }

    // Not indexing, not given rows+columns, so read the channel group config file.
    if (!Opt->readChanGroups()) {
        std::cerr << "Could not read the channel file $HOME/.dastard/channels.json\n";
        std::cerr << "Therefore, you must set row+column counts with -rNR -cNC, or use indexing with -i." << std::endl;
        Opt->failed = true;
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
