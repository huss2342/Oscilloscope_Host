#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QSerialPort>
#include "waveformthread.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void initializeSerialCommunication();

private:
    Ui::MainWindow *ui;
    QSerialPort serial;

    WaveformThread *m_waveformThread;
    OscilloscopeSettings m_oscSettings;
    WaveformData m_waveformData;
    WaveformData m_currentBuffer;
    WaveformData m_sampledData;
    WaveformData m_snapShotData;

    bool m_isSampling;
    double m_triggerLevel;
    double m_gain;
    double m_dataMultiplier;
    bool m_isTrig1Hit;
    bool m_isTrig2Hit;
    double m_zoomLevel;
    bool m_snapShot;

    void setupOscilloscopeControls();
    void applyTriggerSettings();

private slots:
    void onBrowseFile();
    QString isConnected();
    void setRisingEdgeTrigger();
    void setFallingEdgeTrigger();
    void setTriggerLevel();
    void generateWaveformData();
    void onDataSliderChanged(double value);
    void onSnapshot();
    void onClear();
    void onPeek(const QString &addressStr, bool debug = true);
    void onPoke(const QString &addressStr, const QString &dataStr, bool isHex, bool debug = true);
    void onVersion();
    void turnOnBoard();
    void onZoomOut();
    void onDefaultZoom();
    void onZoomIn();
    void onRefreshCOMPorts();
    void updateWaveforms();
    void onUpdateFirmware();
    void updateStatusLabel(const QString &status);
    void logInfo(const QString &message);
    void onStartStopSampling();
    void Sampling();
    void sampleAndUpdateWaveforms();
    void initDMA();
    void onWaveformDrawn();

protected:
    void resizeEvent(QResizeEvent* event) override;
};

#endif // MAINWINDOW_H
