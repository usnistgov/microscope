#ifndef MICROSCOPE_H
#define MICROSCOPE_H

#include <cmath>

#define KILLPORT    "inproc://killthreads" ///< Port for the main thread to kill worker threads
#define CHANSUBPORT "inproc://chansubscriptions" ///< Port for plotter to say which channels are needed

static bool approx_equal(double a, double b, double reltol) {
    return fabs(a-b) < 0.5*(fabs(a)+fabs(b))*reltol;
}

#endif // MICROSCOPE_H
