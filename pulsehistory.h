#ifndef DATACHANNEL_H
#define DATACHANNEL_H

#include <QVector>
#include <QQueue>

class FFTMaster;

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
    pulseHistory(int capacity, FFTMaster *master);

    void clearQueue(int keep=0);
    QVector<double> *newestRecord();
    QVector<double> *newestPSD();
    QVector<double> *meanRecord() const;
    void insertRecord(QVector<double> *r);
    int  size() const;
    int  uses() const {return nstored;}
    void setDoDFT(bool dft);

private:
    int queueCapacity; ///< How long the records and spectra queues should be.
    int nsamples;      ///< How many samples are in the currently stored records.
    int nstored;       ///< How many records have been stored ever.
    bool doDFT;        ///< Whether we are actively doing DFTs on each record.
    QQueue<QVector<double> *> records;  ///< The last N pulse records.
    QQueue<QVector<double> *> spectra;  ///< The last N power spectra.
    void clearSpectra(int keep=0);
    FFTMaster *fftMaster;
    double previous_mean;
};

#endif // DATACHANNEL_H
