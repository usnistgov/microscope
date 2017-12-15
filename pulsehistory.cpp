#include <iostream>
#include "pulsehistory.h"
#include "fftcomputer.h"

///
/// \brief Constructor for a pulse history log of the given capacity.
/// \param capacity
///
pulseHistory::pulseHistory(int capacity, FFTMaster *master) :
    queueCapacity(capacity),
    nsamples(0),
    doDFT(false),
    fftMaster(master)
{
    ;
}


///
/// \brief Clear the stored queues of records and power spectra.
///
void pulseHistory::clearQueue(int keep) {
    if (keep < 0)
        keep = 0;
    while (records.size() > keep) {
        QVector<double> *r = records.dequeue();
        delete r;
    }
    clearSpectra(keep);
}

void pulseHistory::clearSpectra(int keep) {
    if (keep < 0)
        keep = 0;
    while (spectra.size() > keep) {
        QVector<double> *r = spectra.dequeue();
        delete r;
    }
}


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
            std::cout << "Doing fft on record " << i << std::endl;
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
QVector<double> *pulseHistory::newestRecord() {
    if (records.size() <= 0)
        return NULL;
    return records.back();
}


///
/// \brief Return the most recently stored record.
/// \return
///
QVector<double> *pulseHistory::newestPSD() {
    if (records.size() <= 0)
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

    if (nused > 0) {
         for (int j=0; j<nsamples; j++)
            (*result)[j] /= nused;
    }
    return result;
}


///
/// \brief Insert a single triggered record into storage.
/// \param r  The record to store
///
void pulseHistory::insertRecord(QVector<double> *r) {

    // If this record is not the same length as the others, clear out the others.
    int len = r->size();
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
}


///
/// \brief pulseHistory::size
/// \return
///
int pulseHistory::size() const {
    return records.size();
}
