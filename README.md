## MICROSCOPE

Microscope (https://github.com/usnistgov/microscope) is a tool to plot microcalorimeter
pulses in an oscilloscope style, with traces from up to 8 separate channels per panel,
in up to 4 panels. Plots can include:

* Time-series plots of triggered records
* Power spectral densities of triggered records
* Averages of several recent time-series or PSDs

### Installation

As of June 2025, Microscope is a pure-Python program. (It was originally written in C++, starting in
late 2017.) You should install inside a conda environment or a regular python virtual enviroment.
If you need a conda environment, see 
[the installation instructions](https://www.anaconda.com/docs/getting-started/miniconda/install). Make sure
it's installed, and the environment of your choice is active.

```bash
# Install for normal users
pip install git+https://github.com/usnistgov/microscope.git@master


# Install for developers (i.e., you expect to modify the code on your system)
# First change directory to where you'd like the code to reside.
git clone https://github.com/usnistgov/microscope
cd microscope
pip install -e .
```

There are ways for developers to have pip decide where to put the code. You could do that, instead.

**Check that it worked** Now change to an arbitrary directory and make sure you can
run the program:

```
cd ~
microscope
```
