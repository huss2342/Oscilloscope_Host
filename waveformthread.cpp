// waveformthread.cpp
#include "waveformthread.h"

WaveformThread::WaveformThread(QObject *parent)
    : QThread(parent)
{
}

void WaveformThread::run()
{
    m_renderer.analyzeWaveformData(m_waveformData);
    m_renderer.drawWaveform(m_sineLabel, m_waveformData.channel1);
    //m_renderer.drawWaveform(m_squareLabel, m_waveformData.channel2);
    emit waveformDrawn();
}

void WaveformThread::setWaveformData(const WaveformData &data)
{
    m_waveformData = data;
}

void WaveformThread::setLabels(QLabel *sineLabel, QLabel *squareLabel)
{
    m_sineLabel = sineLabel;
    m_squareLabel = squareLabel;
}

void WaveformThread::setOscilloscopeSettings(const OscilloscopeSettings &settings)
{
    m_renderer.setOscilloscopeSettings(settings);
}

void WaveformThread::setZoomLevel(const double m_zoomLevel){
    m_renderer.setZoomLevel(m_zoomLevel);
}
