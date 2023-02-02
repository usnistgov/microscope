## MICROSCOPE

Microscope (https://github.com/usnistgov/microscope) is a tool to plot microcalorimeter
pulses in an oscilloscope style, with traces from up to 8 separate channels. Plots
can include

* Time-series plots of triggered records
* Power spectral densities of triggered records
* Averages of several recent time-series or PSDs
* Scatter plots of analyzed data values vs time
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

#### Download and build

```text
git clone https://github.com/usnistgov/microscope
cd microscope
qmake # (for the system default/preferred Qt version)
# If you need a specific Qt version, use one of:
# qmake-qt4
# qmake-qt5
make
./microscope
```

If you used to use Qt4 and want to switch to Qt5, you first should `make clean` then remove the `Makefile`
generated in the past by the Qt4 version of `qmake` step before running the Qt5 version of `qmake`.

#### Installation

If one of those methods works, then you need to _install_ microscope somewhere in your
path, so that [dastard-commander](https://github.com/usnistgov/dastard-commander)
can start it automatically. We have not yet created a `make install` target, because
`qmake` is super-confusing, so you have two slightly less convenient choices.

First, check your path with `echo $PATH`.
In what follows, we'll assume that you found `/usr/local/bin` to be on your path.

**Method A** If you don't expect to update microscope often, then you might as well copy
the exectuable into your path each time you re-build it, e.g.

```
sudo install -b microscope /usr/local/bin
```

**Method B** If you expect to be always using "the latest" microscope and building
from the git repository often, then you can make a symbolic link once and use it
forever. It will point to the executable file and continue to point at any file
that replaces it by the same name in the same directory:

```
sudo ln -s /home/pcuser/microscope/microscope /usr/local/bin/
```

**Check that it worked** Now change to an arbitrary directory and make sure you can
run the program:

```
cd ~
microscope
```
