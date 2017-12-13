#ifndef DATASUBSCRIBER_H
#define DATASUBSCRIBER_H

#include <zmq.hpp>
#include <QObject>

class QThread;
class plotWindow;

///////////////////////////////////////////////////////////////////////////////
/// Object to run in a private thread,
///
class dataSubscriber : public QObject
{
    Q_OBJECT

public:
    dataSubscriber(plotWindow *w, zmq::context_t *zmqcontext);
    ~dataSubscriber();

    void wait(unsigned long time=ULONG_MAX);

signals:
    /// Signal emitted when this object completes (i.e., its destructor is called)
    void finished(void);
    void newSampleTime(double);
    void newRecordLengths(int,int);

public slots:
    /// Slot to call when it's time to terminate this thread.
    void terminate(void) {emit finished();}
    void process(void);

private:
    QThread *myThread;  ///< The QThread where this object's work is performed
    plotWindow *window; ///< Where we render data
    zmq::context_t *zmqcontext;
    zmq::socket_t *subscriber;
    zmq::socket_t *killsocket;
    zmq::socket_t *chansocket;

    void subscribeChannel(int channum);
    void unsubscribeChannel(int channum);
    void parseChannelMessage(zmq::message_t &);

    double sampletime;
    int nsamples;
    int presamples;
};


class pulseRecord {

public:
    pulseRecord(const zmq::message_t &message);
    ~pulseRecord();

public:
    int channum;
    int presamples;
    int wordsize;
    float sampletime;
    float voltsperarb;
    int nsamples;
    const uint16_t *data;
};


#endif // DATASUBSCRIBER_H
