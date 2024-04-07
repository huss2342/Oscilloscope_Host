// waveformrenderer.cpp
#include "waveformrenderer.h"
#include <QLabel>
#include <QVector>
#include <QPainter>
#include <QPen>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

WaveformRenderer::WaveformRenderer()
    : m_zoomLevel(1.0)
    , m_gain(1.0)
    , m_isTrig1Hit(false)
    , m_isTrig2Hit(false)
{
}

void WaveformRenderer::drawWaveform(QLabel *label, const QVector<double> &data)
{
    if (data.isEmpty()) return;

    // Setup for drawing
    QSize labelSize = label->size();
    QPixmap pixmap(labelSize);
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    QPen pen(Qt::black);
    painter.setPen(pen);

    double xScale = labelSize.width() / static_cast<double>(data.size() - 1);
    double yScale = (labelSize.height() / 2.0) * m_gain / m_zoomLevel;

    QPainterPath path;
    double yPos = labelSize.height() / 2.0 - data[0] * yScale; // Corrected y-position calculation
    path.moveTo(0, yPos);

    for (int i = 1; i < data.size(); ++i) {
        double nextYPos = labelSize.height() / 2.0 - data[i] * yScale; // Corrected y-position calculation
        double xPos = i * xScale;
        path.lineTo(xPos, nextYPos);

        // Rising Edge detection and highlighting
        if (m_oscSettings.triggerType == RisingEdge && data[i] < data[i-1]) {
            painter.setPen(QPen(Qt::red, 2));
            painter.drawEllipse(QPointF(xPos, nextYPos), 2, 2);
            painter.setPen(pen);
        }

        // Falling Edge detection and highlighting
        if (m_oscSettings.triggerType == FallingEdge && data[i] > data[i-1]) {
            painter.setPen(QPen(Qt::blue, 2));
            painter.drawEllipse(QPointF(xPos, nextYPos), 2, 2);
            painter.setPen(pen);
        }
    }

    // Calculate max and min values from data
    double maxVal = *std::max_element(data.constBegin(), data.constEnd());
    double minVal = *std::min_element(data.constBegin(), data.constEnd());

    // Draw max and min values on the graph
    painter.setPen(Qt::black); // Use black pen for text
    painter.drawText(QPointF(5, 20), QString("Max: %1").arg(maxVal)); // Position these based on your UI layout
    painter.drawText(QPointF(5, labelSize.height() - 5), QString("Min: %1").arg(minVal));

    // Draw trigger line if trigger mode is set
    if (m_oscSettings.triggerType == TriggerLevel) {
        double triggerYPos = labelSize.height() / 2.0 - m_oscSettings.triggerLevel * yScale; // Corrected trigger line position
        QPen triggerPen(Qt::darkGreen, 1);
        painter.setPen(triggerPen);
        painter.drawLine(0, triggerYPos, labelSize.width(), triggerYPos);
        painter.setPen(pen);
    }

    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPath(path);
    label->setPixmap(pixmap);
}

//void WaveformRenderer::analyzeWaveformData(const WaveformData &data)
//{
//    bool triggerLevelReachedChannel1 = std::any_of(m_waveformData.channel1.begin(), m_waveformData.channel1.end(), [this](double value) {
//        return m_oscSettings.triggerType == TriggerLevel && std::abs(value) >= m_oscSettings.triggerLevel;
//    });

//    bool triggerLevelReachedChannel2 = std::any_of(m_waveformData.channel2.begin(), m_waveformData.channel2.end(), [this](double value) {
//        return m_oscSettings.triggerType == TriggerLevel && std::abs(value) >= m_oscSettings.triggerLevel;
//    });

//    if (triggerLevelReachedChannel1 && !m_isTrig1Hit) {
//        m_isTrig1Hit = true;
//        m_snapShotData.channel1 = m_waveformData.channel1;
//    }

//    if (triggerLevelReachedChannel2 && !m_isTrig2Hit) {
//        m_isTrig2Hit = true;
//        m_snapShotData.channel2 = m_waveformData.channel2;
//    }
//}

void WaveformRenderer::analyzeWaveformData(const WaveformData &data)
{
    bool triggerLevelReachedChannel1 = std::any_of(data.channel1.begin(), data.channel1.end(), [this](double value) {
        return m_oscSettings.triggerType == TriggerLevel && std::abs(value) >= m_oscSettings.triggerLevel;
    });

    bool triggerLevelReachedChannel2 = std::any_of(data.channel2.begin(), data.channel2.end(), [this](double value) {
        return m_oscSettings.triggerType == TriggerLevel && std::abs(value) >= m_oscSettings.triggerLevel;
    });

    if (triggerLevelReachedChannel1 && !m_isTrig1Hit) {
        m_isTrig1Hit = true;
        m_snapShotData.channel1 = data.channel1;
    }

    if (triggerLevelReachedChannel2 && !m_isTrig2Hit) {
        m_isTrig2Hit = true;
        m_snapShotData.channel2 = data.channel2;
    }
}

void WaveformRenderer::setWaveformData(const WaveformData &data)
{
    m_waveformData = data;
}

void WaveformRenderer::setZoomLevel(double level)
{
    m_zoomLevel = level;
}

void WaveformRenderer::setGain(double gain)
{
    m_gain = gain;
}

void WaveformRenderer::setIsTrig1Hit(bool hit)
{
    m_isTrig1Hit = hit;
}

void WaveformRenderer::setIsTrig2Hit(bool hit)
{
    m_isTrig2Hit = hit;
}

void WaveformRenderer::setSnapShotData(const WaveformData &data)
{
    m_snapShotData = data;
}

void WaveformRenderer::setOscilloscopeSettings(const OscilloscopeSettings &settings)
{
    m_oscSettings = settings;
}
