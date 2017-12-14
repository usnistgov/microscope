#include "pulsehistory.h"

///
/// \brief Constructor for data channel of the given capacity.
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
    records.clear();
    spectra.clear();
}


///
/// \brief Return the most recently stored record.
/// \return
///
QVector<double>& pulseHistory::newestRecord() {
    return records.back();
}


///
/// \brief Compute and return the mean of all stored records.
/// \return
///
QVector<double> pulseHistory::meanRecord() const {
    QVector<double> last = records.back();
    int len = last.size();
    QVector<double> result(len, 0.0);

    int nused = 0;
    for (int i=0; i<records.size(); i++) {
        if (records[i].size() == len) {
            for (int j=0; j<len; j++)
                result[j] += records[i][j];
            nused++;
        }
    }
    if (nused > 0) {
         for (int j=0; j<len; j++)
            result[j] /= nused;
    }
    return result;
}


///
/// \brief Insert a single triggered record into storage.
/// \param r  The record to store
///
void pulseHistory::insertRecord(QVector<double> r) {

    // If this record is not the same length as the others, clear out the others.
    int len = r.size();
    if (len != nsamples) {
        nsamples = len;
        clearQueue();
    }

    // Now add this record and trim to size.
    records.enqueue(r);
    while (records.size() > queueCapacity)
        records.dequeue();
}
