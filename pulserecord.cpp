#include "pulserecord.h"

///////////////////////////////////////////////////////////////////////////////
/// \brief pulseRecord::pulseRecord
/// \param message
///
pulseRecord::pulseRecord(const zmq::message_t &header, const zmq::message_t &pulsedata) {
    const size_t prefix_size = 36;
    assert (prefix_size <= header.size());

    const char* msg = reinterpret_cast<const char *>(header.data());
    channum = *reinterpret_cast<const uint16_t *>(&msg[0]);
    char version = msg[2];
    assert (version == 0);

    // Ignore word size code for now (assert uint16 or int16)
    char wordsizecode = msg[3];
    assert (wordsizecode == 3 || wordsizecode == 2);
    bool issigned = (wordsizecode == 2);
    wordsize = 2;

    presamples = *reinterpret_cast<const uint32_t *>(&msg[4]);
    nsamples = *reinterpret_cast<const uint32_t *>(&msg[8]);

    sampletime = *reinterpret_cast<const float *>(&msg[12]);
    voltsperarb = *reinterpret_cast<const float *>(&msg[16]);

    time_nsec = *reinterpret_cast<const uint64_t *>(&msg[20]);
    serialnumber = *reinterpret_cast<const uint64_t *>(&msg[28]);

    assert (nsamples*wordsize == int(pulsedata.size()));
    const uint16_t *u16data = reinterpret_cast<const uint16_t *>(pulsedata.data());
    const int16_t *i16data = reinterpret_cast<const int16_t *>(pulsedata.data());

    data = new QVector<double>;
    data->resize(nsamples);
    if (issigned) {
        for (int i=0; i<nsamples; i++)
            (*data)[i] = i16data[i];
    } else {
        for (int i=0; i<nsamples; i++)
            (*data)[i] = u16data[i];
    }
}

// copy constructor
pulseRecord::pulseRecord(const pulseRecord &pr) {
    channum = pr.channum;
    presamples = pr.presamples;
    wordsize = pr.wordsize;
    sampletime = pr.sampletime;
    voltsperarb = pr.voltsperarb;
    nsamples = pr.nsamples;
    time_nsec = pr.time_nsec;
    serialnumber = pr.serialnumber;
    data = pr.data;
}

pulseRecord::pulseRecord(const QVector<double> *data_in) {
    nsamples = data_in->size();
    data = new QVector<double>(nsamples);
    for (int i=0; i<nsamples; i++)
        (*data)[i] = data_in->at(i);
}


pulseRecord::~pulseRecord() {
    delete data;
}

