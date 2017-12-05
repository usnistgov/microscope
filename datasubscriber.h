#ifndef DATASUBSCRIBER_H
#define DATASUBSCRIBER_H

#include <QObject>

class QThread;

///////////////////////////////////////////////////////////////////////////////
/// Object to run in a private thread,
///
class dataSubscriber : public QObject
{
    Q_OBJECT

public:
    dataSubscriber();
    ~dataSubscriber();

signals:
    /// Signal emitted when this object completes (i.e., its destructor is called)
    void finished(void);

public slots:
    /// Slot to call when it's time to terminate this thread.
    void terminate(void) {emit finished();}
    void process(void);

private:
    QThread *myThread;  ///< The QThread where this object's work is performed
};

#endif // DATASUBSCRIBER_H