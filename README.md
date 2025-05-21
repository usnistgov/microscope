## MICROSCOPE

Microscope (https://github.com/usnistgov/microscope) is a tool to plot microcalorimeter
pulses in an oscilloscope style, with traces from up to 8 separate channels. Plots
can include:

* Time-series plots of triggered records
* Power spectral densities of triggered records
* Averages of several recent time-series or PSDs
* (not implemented yet) scatter plots of analyzed data values vs time
* (not implemented yet) histograms of analyzed data

### Installation from git

To proceed, you will need to have installed:
* git
* gcc and make (possibly other development tools I'm forgetting?)
* zmq (NOT necessarily including zmq.hpp--the C++ binding--because we have our own copy of that)
* Qt5 (or Qt4) development libraries (we encourage Qt5)

On an Ubuntu 18.04 or 20.04 system, this can be accomplished by a command along these lines:

```
sudo apt-get install gcc make git qt5-default qt5-qmake libzmq3-dev libfftw3-dev
#
# If you really need qt4 for other reasons, then replace the above with:
# sudo apt-get install gcc make git qt4-default qt4-qmake libzmq3-dev libfftw3-dev

# If you want to work on (edit) the program, you probably also want:
sudo apt-get install qtcreator qtchooser
```

On MacPorts, you need something like the following:
```
sudo port install qt5 qt5-base gcc10 gmake git zmq fftw-3
# If you want to work on the program, you probably also want:
sudo port install qt5-qtcreator
```

#### Download and install

```bash
# Install for normal users

# Install for developers (i.e., you expect to modify the code on your system)
git clone https://github.com/usnistgov/microscope
cd microscope
pip install -e .
```

**Check that it worked** Now change to an arbitrary directory and make sure you can
run the program:

```
cd ~
microscope
```
