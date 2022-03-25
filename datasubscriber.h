#ifndef DATASUBSCRIBER_H
#define DATASUBSCRIBER_H

#include <zmq.hpp>
#include <QObject>
#include <QVector>

class QThread;
class plotWindow;
class refreshPlots;
class pulseRecord;

///////////////////////////////////////////////////////////////////////////////
/// Object to run in a private thread,
///
class dataSubscriber : public QObject
{
    Q_OBJECT

public:
    dataSubscriber(plotWindow *w, zmq::context_t *zmqcontext, std::string tcpsource);
    ~dataSubscriber();

    void wait(unsigned long time=ULONG_MAX);

signals:
    /// Signal emitted when this object completes (i.e., its destructor is called)
    void finished(void);

    /// Signal emitted when this object fails to run (e.g., cannot reach host)
    void failed(void);

    void newSampleTime(double);

    /// Signal that a data vector is ready to plot
    void newDataToPlot(int channum, pulseRecord *pr);


public slots:
    /// Slot to call when it's time to terminate this thread.
    void terminate(void) {emit finished();}
    void process(void);

private:
    QThread *myThread;  ///< The QThread where this object's work is performed
    plotWindow   *window;      ///< Where we the data are plotted
    refreshPlots *plotManager; ///< Where we send data for plotting
    zmq::context_t *zmqcontext;
    zmq::socket_t *subscriber;
    std::string tcpdatasource;

    void subscribeChannel(int channum);
    void unsubscribeChannel(int channum);
    void parseChannelMessage(zmq::message_t &);

    double sampletime;
};

// The API for zmq::socket_t changes in version 4.7.0+ but only if you're using C++11 or later.
#if (defined(ZMQ_CPP11) && (CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4, 7, 0)))
#define USE_NEW_SOCKET_SET_API
#endif

#endif // DATASUBSCRIBER_H
