#include <iostream>
#include <math.h>

#include "pulsehistory.h"
#include "fftcomputer.h"

///
/// \brief Constructor for a pulse history log of the given capacity.
/// \param capacity
///
pulseHistory::pulseHistory(int capacity, FFTMaster *master) :
    queueCapacity(capacity),
    nsamples(0),
    nstored(0),
    doDFT(false),
    fftMaster(master)
{
    ;
}


///
/// \brief shorten_from_front Remove all but the last `keep` values from the vector, moving them to the front.
///
/// Notice that resize won't work, because it removes values from the back, not the front.
/// \param v   Vector to shorten
/// \param keep  How many values to keep.
///
inline void shorten_from_front(QVector<double> &v, int keep) {
    if (keep == 0) {
        v.resize(0);
    } else if (keep > v.size())
        return;
    else
        v.remove(0, v.size()-keep);
}


///
/// \brief Clear the stored queues of records, power spectra, and analysis.
///
void pulseHistory::clearQueue(int keep) {
    if (keep < 0)
        keep = 0;
    while (records.size() > keep) {
        QVector<double> *r = records.dequeue();
        delete r;
    }
    Q_ASSERT(records.size() <= keep);
    clearSpectra(keep);

    shorten_from_front(pulse_average, keep);
    shorten_from_front(pulse_peak, keep);
    shorten_from_front(pulse_rms, keep);
}


///
/// \brief pulseHistory::clearSpectra
/// \param keep
///
void pulseHistory::clearSpectra(int keep) {
    if (keep < 0)
        keep = 0;
    while (spectra.size() > keep) {
        QVector<double> *r = spectra.dequeue();
        delete r;
    }
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
        // run DFT on all data
        int n = records.size();
        if (n <= 0)
            return;
        if (previous_mean == 0.0)
            previous_mean = (*records[0])[0];
        for (int i=0; i<n; i++) {
            QVector<double> *psd = new QVector<double>;
            QVector<double> *data = records[i];
            const double sampleRate = 1.0;
            fftMaster->computePSD(*data, *psd, sampleRate, WINDOW, previous_mean);
        }
    } else {
        clearSpectra();
    }
}


///
/// \brief Return the most recently stored record.
/// \return
///
QVector<double> *pulseHistory::newestRecord() const {
    if (records.isEmpty())
        return NULL;
    return records.back();
}


///
/// \brief Return the most recently stored record.
/// \return
///
QVector<double> *pulseHistory::newestPSD() const {
    if (spectra.isEmpty())
        return NULL;
    return spectra.back();
}


///
/// \brief Compute and return the mean of all stored records.
/// \return
///
QVector<double> *pulseHistory::meanRecord() const {
    QVector<double> *last = records.back();
    if (last == NULL) {
        return NULL;
    }

    QVector<double> *result = new QVector<double>(nsamples, 0.0);

    int nused = 0;
    for (int i=0; i<records.size(); i++) {
        if (records[i]->size() <= nsamples) {
            for (int j=0; j<nsamples; j++)
                (*result)[j] += (*records[i])[j];
            nused++;
        }
    }

    if (nused > 1) {
         for (int j=0; j<nsamples; j++)
            (*result)[j] /= nused;
    }
    return result;
}


///
/// \brief pulseHistory::meanPSD
/// \return
///
QVector<double> *pulseHistory::meanPSD() const {
    QVector<double> *last = spectra.back();
    if (last == NULL) {
        return NULL;
    }

    int nfreq = last->size();
    QVector<double> *result = new QVector<double>(nfreq, 0.0);

    int nused = 0;
    for (int i=0; i<spectra.size(); i++) {
        if (spectra[i]->size() <= nfreq) {
            for (int j=0; j<nfreq; j++)
                (*result)[j] += (*spectra[i])[j];
            nused++;
        }
    }

    if (nused > 1) {
         for (int j=0; j<nfreq; j++)
            (*result)[j] /= nused;
    }
    return result;
}


///
/// \brief Insert a single triggered record into storage.
/// \param r  The record to store
///
void pulseHistory::insertRecord(QVector<double> *r, int presamples) {

    // If this record is not the same length as the others, clear out the others.
    const int len = r->size();
    if (len != nsamples) {
        nsamples = len;
        clearQueue();
    }

    // Now add this record and trim to size.
    nstored++;
    clearQueue(queueCapacity-1);
    records.enqueue(r);

    if (doDFT) {
        const bool WINDOW=true; // always use Hann windowing
        QVector<double> *psd = new QVector<double>();
        fftMaster->computePSD(*r, *psd, 1.0, WINDOW, previous_mean);
        spectra.enqueue(psd);
    }

    // Now compute and store its "analysis" values
    QVector<double> rec = *r;
    double ptmean = 0.0;
    for (int i=0; i<presamples; i++)
        ptmean += rec[i];
    ptmean /= presamples;

    double pavg = 0.0;
    double peak = 0.0;
    for (int i=presamples+1; i<len; i++) {
        pavg += rec[i];
        if (rec[i] > peak)
            peak = rec[i];
    }
    pavg = pavg/(len-presamples-1) - ptmean;
    peak -= ptmean;

    double sumsq = 0.0;
    for (int i=presamples+1; i<len; i++) {
        double v = rec[i]-ptmean;
        sumsq += v*v;
    }
    double prms = sqrt(sumsq/(len-presamples-1));

    pulse_average.append(pavg);
    pulse_peak.append(peak);
    pulse_rms.append(prms);
}


///
/// \brief pulseHistory::size
/// \return
///
int pulseHistory::size() const {
    return records.size();
}
