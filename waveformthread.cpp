// waveformthread.cpp
#include "waveformthread.h"
#include <QMutex>
#include <QWaitCondition>

WaveformThread::WaveformThread(QObject *parent)
    : QThread(parent)
{
}

void WaveformThread::run()
{
    dataMutex.lock();
    m_renderer.analyzeWaveformData(m_waveformData);
    m_renderer.drawWaveform(m_sineLabel, m_waveformData.channel1);
    //m_renderer.drawWaveform(m_squareLabel, m_waveformData.channel2);
    dataMutex.unlock();
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

bool WaveformThread::is_m_isTrig1Hit(){
    return m_renderer.m_isTrig1Hit;
}

bool WaveformThread::is_m_isTrig2Hit(){
    return m_renderer.m_isTrig2Hit;
}

void WaveformThread::set_m_isTrig1Hit(bool set){
    m_renderer.m_isTrig1Hit = set;
}

void WaveformThread::set_m_isTrig2Hit(bool set){
    m_renderer.m_isTrig2Hit = set;
}

WaveformData WaveformThread::getTrigWave(){
    return m_renderer.m_snapShotData;
}

void WaveformThread::setShiftValue(qint32 shiftValue) {
    m_renderer.m_shiftValue = shiftValue;
}

