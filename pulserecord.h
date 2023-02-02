#ifndef PULSERECORD_H
#define PULSERECORD_H

#include <QVector>
#include "zmq.hpp"

class pulseRecord {

public:
    pulseRecord(const zmq::message_t &header, const zmq::message_t &pulsedata);
    pulseRecord(const pulseRecord &pr);
    pulseRecord(const QVector<double> &data);
    pulseRecord();
    ~pulseRecord();

public:
    int channum;
    int presamples;
    int wordsize;
    double sampletime;
    double voltsperarb;
    int nsamples;
    uint64_t time_nsec;
    uint64_t serialnumber;
    double dtime;  // when it happened for timeseries plots
    QVector<double> data;
};



#endif // PULSERECORD_H
