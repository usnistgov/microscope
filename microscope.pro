#-------------------------------------------------
#
# Project created by QtCreator 2014-03-14T16:52:11
#
#-------------------------------------------------

QT       += core gui  network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

TARGET = microscope
TEMPLATE = app
CONFIG -= app_bundle
CONFIG += qt thread

LIBS += -lpthread -lfftw3 -lzmq # -lpthread -lX11 -lm -lXext -ldl -lfftw3 -lzmq
QMAKE_CXXFLAGS += -std=c++0x
QMAKE_CFLAGS_RELEASE += -O3 -g
QMAKE_CXXFLAGS_RELEASE += -O3 -g
QMAKE_CFLAGS_DEBUG += -O0
QMAKE_CXXFLAGS_DEBUG += -O0



##################################################
# The following are MacPorts only:
macx {
  QMAKE_CC = /opt/local/bin/gcc
  QMAKE_CXX = /opt/local/bin/g++
  QMAKE_LINK = /opt/local/bin/g++
  QMAKE_CFLAGS_X86_64 -= -Xarch_x86_64
  QMAKE_CXXFLAGS_X86_64 -= -Xarch_x86_64
  QMAKE_CXXFLAGS -= -stdlib=libc++
  QMAKE_LFLAGS -= -stdlib=libc++
  QMAKE_LFLAGS_X86_64 -= -Xarch_x86_64
  QMAKE_CFLAGS += -I/opt/local/include
  QMAKE_CXXFLAGS += -I/opt/local/include
  QMAKE_LFLAGS += -L/opt/local/lib
}
# end MacPorts only.
##################################################


SOURCES += main.cpp\
    periodicupdater.cpp \
    plotwindow.cpp \
    qcustomplot.cpp \
    refreshplots.cpp \
    datasubscriber.cpp \
    pulsehistory.cpp \
    fftcomputer.cpp \
    pulserecord.cpp

HEADERS  += plotwindow.h \
    periodicupdater.h \
    qcustomplot.h \
    refreshplots.h \
    version.h \
    datasubscriber.h \
    microscope.h \
    pulsehistory.h \
    fftcomputer.h \
    pulserecord.h

FORMS    +=  plotwindow.ui

# Doxygen: custom target 'doc'
# dox.target = dox
# dox.depends = $(SOURCES) $(HEADERS) doc/doxygen.conf
# dox.commands = doxygen doc/doxygen.conf
# QMAKE_EXTRA_TARGETS += dox

OTHER_FILES += \
    .gitignore
