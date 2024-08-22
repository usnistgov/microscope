#include <iostream>
#include <math.h>

#include <ctime>
#include "pulsehistory.h"
#include "datasubscriber.h"
#include "pulserecord.h"
#include "fftcomputer.h"

///
/// \brief Constructor for a pulse history log of the given capacity.
/// \param capacity
///
pulseHistory::pulseHistory(int capacity, FFTMaster *master) :
    queueCapacity(capacity),
    analysisHardCap(20000),
    analysisSoftCap(16000),
    nsamples(0),
    nstored(0),
    doDFT(false),
    fftMaster(master)
{

}

pulseHistory::~pulseHistory() {
    clearAllData();
}


///
/// \brief Clear the stored queues of records, power spectra, and analysis.
///
void pulseHistory::clearAllData() {
    pulse_average.clear();
    pulse_peak.clear();
    pulse_rms.clear();
    pulse_time.clear();

    const int RESERVE=analysisHardCap; // reserve space for this many values--a memory optimization tactic
    // Note that the QVector::reserve statements do NOT set a hard limit on the QVector size, but only a
    // hint to the QVector memory manager.
    pulse_average.reserve(RESERVE);
    pulse_peak.reserve(RESERVE);
    pulse_rms.reserve(RESERVE);
    pulse_time.reserve(RESERVE);

    clearQueue();
}



///
/// \brief Clear the stored queues of records and power spectra.
///
void pulseHistory::clearQueue(int keep) {
    if (keep < 0)
        keep = 0;
    lock.lock();
    while (records.size() > keep) {
        pulseRecord *pr = records.dequeue();
        delete pr;
    }
    lock.unlock();
    Q_ASSERT(records.size() <= keep);

    clearSpectra(keep);
}


///
/// \brief pulseHistory::clearSpectra
/// \param keep
///
void pulseHistory::clearSpectra(int keep) {
    if (keep < 0)
        keep = 0;
    lock.lock();
    while (spectra.size() > keep) {
        QVector<double> *sp = spectra.dequeue();
        delete sp;
    }
    lock.unlock();
    Q_ASSERT(spectra.size() <= keep);
}


///
/// \brief pulseHistory::setDoDFT
/// \param dft
///
void pulseHistory::setDoDFT(bool dft) {
    if (doDFT == dft)
        return;
    doDFT=dft;
    const bool WINDOW=true; // always use Hann windowing

    if (dft) {
        // run DFT on all data already in queue
        int n = records.size();
        if (n <= 0)
            return;
        if (previous_mean == 0.0)
            previous_mean = records[0]->data[0];
        lock.lock();
        for (int i=0; i<n; i++) {
            QVector<double> *psd = new QVector<double>;
            const double sampleRate = 1.0;
            fftMaster->computePSD(records[i]->data, *psd, sampleRate, WINDOW, previous_mean);
            spectra.append(psd);
        }
        lock.unlock();
    } else {
        clearSpectra();
    }
}


///
/// \brief Return the most recently stored record.
/// \return
///
pulseRecord *pulseHistory::newestRecord() const {
    if (records.isEmpty())
        return nullptr;
    return records.back();
}


///
/// \brief Return the most recently stored power spectrum.
/// \return
///
QVector<double> *pulseHistory::newestPSD() const {
    if (spectra.isEmpty())
        return nullptr;
    return spectra.back();
}


///
/// \brief Compute and return the mean of the last 'nAverage' stored records.
/// \return
///
pulseRecord *pulseHistory::meanRecord(int nAverage) {
    lock.lock(); // lock before accessing records

    pulseRecord *last = newestRecord();
    if (last == nullptr) {
        lock.unlock();
        return nullptr;
    }
    pulseRecord *result = new pulseRecord(*last);

    int nused = 1; //The last record is included for free above

    int start = std::max(records.size() - nAverage, 0);
    for (int i=start; i<records.size() - 1; i++) {
        if (records[i]->nsamples == nsamples) {
            for (int j=0; j<nsamples; j++)
                (result->data)[j] += records[i]->data[j];
            nused++;
        }
    }
    lock.unlock();

    if (nused > 1) {
         for (int j=0; j<nsamples; j++)
            (result->data)[j] /= nused;
    }
    return result;
}


///
/// \brief Compute and return the mean of the last 'nAverage' stored PSDs
/// \return
///
QVector<double> *pulseHistory::meanPSD(int nAverage) {
    lock.lock();
    QVector<double> *last = newestPSD();
    if (last == nullptr) {
        lock.unlock();
        return nullptr;
    }

    int nfreq = last->size();
    mean_psd.resize(nfreq);
    for (int i=0; i<nfreq; i++)
        mean_psd[i] = 0;

    int nused = 0; // the base record was zeroed out above, different from pulseHistory::meanRecord

    int start = std::max(records.size() - nAverage, 0);
    for (int i=start; i<spectra.size(); i++) {
        if (spectra[i]->size() == nfreq) {
            for (int j=0; j<nfreq; j++)
                mean_psd[j] += (*spectra[i])[j];
            nused++;
        }
    }
    lock.unlock();

    if (nused > 1) {
         for (int j=0; j<nfreq; j++)
            mean_psd[j] /= nused;
    }
    return &mean_psd;
}


///
/// \brief Insert a single triggered record into storage.
/// \param r  The record to store
///
void pulseHistory::insertRecord(pulseRecord *pr) {

    // If this record is not the same length as the others, clear out the others.
    const int len = pr->nsamples;
    if (len != nsamples) {
        nsamples = len;
        clearAllData();
    }

    // Now add this record and trim to size.
    clearQueue(queueCapacity-1);
    lock.lock();
    nstored++;
    records.enqueue(pr);
    lock.unlock();

    if (doDFT) {
        clearSpectra(queueCapacity-1);
        const bool WINDOW=true; // always use Hann windowing
        QVector<double> *psd = new QVector<double>();
        fftMaster->computePSD(pr->data, *psd, 1.0, WINDOW, previous_mean);
        lock.lock();
        spectra.enqueue(psd);
        lock.unlock();
    }

    // Now compute and store its "analysis" values
    lock.lock();
    QVector<double> rec = pr->data;
    double ptmean = 0.0;
    for (int i=0; i<pr->presamples; i++)
        ptmean += rec[i];
    ptmean /= pr->presamples;

    double pavg = 0.0;
    double peak = 0.0;
    for (int i=pr->presamples+1; i<len; i++) {
        pavg += rec[i];
        if (rec[i] > peak)
            peak = rec[i];
    }
    pavg = pavg/(len-pr->presamples-1) - ptmean;
    peak -= ptmean;

    double sumsq = 0.0;
    for (int i=pr->presamples+1; i<len; i++) {
        double v = rec[i]-ptmean;
        sumsq += v*v;
    }
    double prms = sqrt(sumsq/(len-pr->presamples-1));
    lock.unlock();

    if (pulse_average.size() >= analysisHardCap) {
        const int nval_to_remove = analysisHardCap - analysisSoftCap;
        pulse_average.remove(0, nval_to_remove);
        pulse_peak.remove(0, nval_to_remove);
        pulse_rms.remove(0, nval_to_remove);
        pulse_time.remove(0, nval_to_remove);
        pulse_baseline.remove(0, nval_to_remove);
    }
    pulse_average.append(pavg);
    pulse_peak.append(peak);
    pulse_rms.append(prms);
    pulse_time.append(pr->dtime);
    pulse_baseline.append(ptmean);
}


///
/// \brief pulseHistory::size
/// \return
///
int pulseHistory::size() const {
    return records.size();
}
