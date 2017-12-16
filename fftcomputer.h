#ifndef FFTCOMPUTER_H
#define FFTCOMPUTER_H

#include <fftw3.h>
#include <QVector>
#include <QHash>

///
/// \brief The FFTComputer class
///
class FFTComputer {
public:
    FFTComputer();
    ~FFTComputer();

    void prepare(int length_in);
    void computePSD(QVector<double> &data, QVector<double> &psd, double sampleRate,
                    bool useWindow, double &mean);

private:
    int length;
    int nfreq;

    fftw_plan    plan;
    double       *fftIn;                ///< pointer to the input buffer that you want transformed.
    double       *fftOut;               ///< pointer to the output buffer that gets the transform of in.
    double       *window;
    bool         plan_made;
};



///
/// \brief The FFTMaster class
///
class FFTMaster
{
public:
    FFTMaster();
    void computePSD(QVector<double> &data, QVector<double> &psd, double sampleRate,
                    bool useWindow, double &mean);

private:
    QHash<int, FFTComputer> computers; ///< Multiple FFTComputer objects, one per desired length.
};

#endif // FFTCOMPUTER_H
