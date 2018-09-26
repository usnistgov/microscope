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
* zmq including zmq.hpp (the C++ binding)
* Qt4 development libraries (Qt5 might work, but I doubt it)

On an Ubuntu 16.04 system, this can be accomplished by a command along these lines:

```
sudo apt-get install gcc make git qt4-default qt4-qmake libczmq-dev libzmqpp-dev
# And if you want to work on the program, you probably also want:
sudo apt-get install qtcreator qtchooser
```

#### Download and build

```text
git clone https://github.com/usnistgov/microscope
cd microscope
qmake-qt4
make
./microscope
```

#### Installation

If one of those methods works, then you need to _install_ microscope somewhere in your 
path, so that [dastard-commander](https://github.com/usnistgov/dastard-commander)
can start it automatically. We have not yet created a `make install` target, because
`qmake` is super-confusing, so you have two slightly less convenient choices.

First, check your path with `echo $PATH`.
In this document, we'll assume that you found `/usr/local/bin` to be on your path.

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
sudo ln -s microscope /usr/local/bin
```

**Check that it worked** Now change to an arbitrary directory and make sure you can 
run the program:

```
cd ~
microscope
```
