// waveformthread.h
#ifndef WAVEFORMTHREAD_H
#define WAVEFORMTHREAD_H

#include <QThread>
#include "waveformrenderer.h"
#include <QMutex>
#include <QWaitCondition>

class WaveformThread : public QThread
{
    Q_OBJECT

public:
    explicit WaveformThread(QObject *parent = nullptr);
    void run() override;

    void setWaveformData(const WaveformData &data);
    void setLabels(QLabel *sineLabel, QLabel *squareLabel);
    void setOscilloscopeSettings(const OscilloscopeSettings &settings);
    void setZoomLevel(const double m_zoomLevel);
    bool is_m_isTrig1Hit();
    bool is_m_isTrig2Hit();

    void set_m_isTrig1Hit(bool set);
    void set_m_isTrig2Hit(bool set);

    WaveformData getTrigWave();
    void setShiftValue(qint32 shiftValue);

signals:
    void waveformDrawn();

private:
    WaveformData m_waveformData;
    QLabel *m_sineLabel;
    QLabel *m_squareLabel;
    WaveformRenderer m_renderer;
    QMutex dataMutex;
    QWaitCondition dataCondition;
};

#endif // WAVEFORMTHREAD_H
