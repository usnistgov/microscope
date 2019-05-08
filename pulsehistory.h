#ifndef DATACHANNEL_H
#define DATACHANNEL_H

#include <QVector>
#include <QQueue>
#include <QMutex>

class FFTMaster;
class pulseRecord;

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

    void insertRecord(pulseRecord *pr);
    void clearAllData();
    pulseRecord *newestRecord() const;
    pulseRecord *meanRecord(int);
    QVector<double> *newestPSD() const;
    QVector<double> *meanPSD(int);
    int  size() const;
    int  uses() const {return nstored;}
    int  samples() const {return nsamples;}
    void setDoDFT(bool dft);

    QVector<double> &rms() {return pulse_rms;}
    QVector<double> &mean() {return pulse_average;}
    QVector<double> &peak() {return pulse_peak;}
    QVector<double> &times() {return pulse_time;}
    QVector<double> &baseline() {return pulse_baseline;}

private:
    int queueCapacity; ///< How long the records and spectra queues should be.
    int nsamples;      ///< How many samples are in the currently stored records.
    int nstored;       ///< How many records have been stored ever.
    bool doDFT;        ///< Whether we are actively doing DFTs on each record.
    QQueue<pulseRecord *> records;  ///< The last N pulse records.
    QQueue<QVector<double> *> spectra;  ///< The last N power spectra.
    FFTMaster *fftMaster;
    double previous_mean;
    QMutex lock;

    // Analysis of single records
    QVector<double> pulse_rms;
    QVector<double> pulse_peak;
    QVector<double> pulse_average;
    QVector<double> pulse_time;
    QVector<double> pulse_baseline;
    QVector<double> mean_psd;

    void clearQueue(int keep=0);
    void clearSpectra(int keep=0);
};

#endif // DATACHANNEL_H
