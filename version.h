////////////////////////////////////////////////////////////////////////////////
/// \file version.h  Give the microscope verion number x.y.z.

#ifndef VERSION_MAJOR
#define VERSION_MAJOR 0       ///< Major version #
#define VERSION_MINOR 1       ///< Minor version #
#define VERSION_REALLYMINOR 8 ///< Version release #
#endif
/*
  0.1.8 2024-07 Christopher Rooney. Average the most recent records instead of the oldest, and other bug fixes for averaging.
  0.1.7 2024-07 Joe Fowler. Make microscope -h return 0, not error.
  0.1.6 2023-01 Joe Fowler. Include zmq.hpp directly to ensure latest cppzmq.
  0.1.5 2022-03 Joe Fowler. Fix error in use of new API in cppzmq 4.7.0+.
  0.1.4 2022-03 Joe Fowler. Update to QCustomPlot 2.1.0. Remove deprecated API of cppzmq 4.7.0+.
  0.1.3 2021-02 Joe Fowler. Fix an err/FB ordering problem. Allow chan 0 to be valid.
  0.1.2 2020-09 Joe Fowler. Use channel groups, instead of rows/columns.
  0.1.1 2019-06 Joe Fowler. Finite amount of analysis stored; memory leaks attacked.
  0.1.0 2018-10 Joe Fowler. Handle TDM/non-TDM separately via command-line args.
  0.0.3 2018-07 Joe Fowler. Fix double-free crashes.
  0.0.2 2018-07 Joe Fowler. Remove unneeded code left from matter.
  0.0.1 2018-07 Joe Fowler. Working towards Dastard 0.1
  0.0.0 2017-12 Joe Fowler

*/
