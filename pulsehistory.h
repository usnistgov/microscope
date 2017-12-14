#ifndef DATACHANNEL_H
#define DATACHANNEL_H

#include <QVector>
#include <QQueue>

/// \file datachannel.h defines the dataChannel object.

///
/// \brief An object to store a brief history of pulse records.
///
/// Stores a queue of the most recent pulse records for a given channel and, if needed,
/// of their power-spectral-density.
///
///
class pulseHistory
{
public:
    pulseHistory(int capacity);

    void clearQueue();
    QVector<double> *newestRecord();
    QVector<double> *meanRecord() const;
    void insertRecord(QVector<double> *r);
    int  size() const;

private:
    int queueCapacity; ///< How long the records and spectra queues should be.
    int nsamples;      ///< How many samples are in the currently stored records.
    bool doDFT;        ///< Whether we are actively doing DFTs on each record.
    QQueue<QVector<double> *> records;  ///< The last N pulse records.
    QQueue<QVector<double> *> spectra;  ///< The last N power spectra.
};

#endif // DATACHANNEL_H
