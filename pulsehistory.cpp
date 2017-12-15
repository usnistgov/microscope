#include <iostream>
#include "pulsehistory.h"

///
/// \brief Constructor for a pulse history log of the given capacity.
/// \param capacity
///
pulseHistory::pulseHistory(int capacity) :
    queueCapacity(capacity),
    nsamples(0),
    doDFT(false)
{
    ;
}


///
/// \brief Clear the stored queues of records and power spectra.
///
void pulseHistory::clearQueue() {
    while (!records.isEmpty()) {
        QVector<double> *r = records.dequeue();
        delete r;
    }
    while (!spectra.isEmpty()) {
        QVector<double> *r = spectra.dequeue();
        delete r;
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
    records.enqueue(r);
    while (records.size() > queueCapacity) {
        QVector<double> *r = records.dequeue();
        delete r;
    }


}


///
/// \brief pulseHistory::size
/// \return
///
int pulseHistory::size() const {
    return records.size();
}
