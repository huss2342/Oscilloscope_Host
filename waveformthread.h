// waveformthread.h
#ifndef WAVEFORMTHREAD_H
#define WAVEFORMTHREAD_H

#include <QThread>
#include "waveformrenderer.h"

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

signals:
    void waveformDrawn();

private:
    WaveformData m_waveformData;
    QLabel *m_sineLabel;
    QLabel *m_squareLabel;
    WaveformRenderer m_renderer;
};

#endif // WAVEFORMTHREAD_H
