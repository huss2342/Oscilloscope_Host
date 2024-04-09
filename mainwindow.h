//******** mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QSerialPort>



QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


// two channels for data
struct WaveformData {
    QVector<double> channel1;
    QVector<double> channel2;
};

enum TriggerType {
    NoTrigger,
    RisingEdge,
    FallingEdge,
    TriggerLevel
};

struct OscilloscopeSettings {
    TriggerType triggerType;
    double triggerLevel;
};


class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void initializeSerialCommunication();

private:
    Ui::MainWindow *ui;
    QSerialPort serial;

    QTimer* updateTimer;
    bool isRisingEdgeFound = false;
    double previousLockingLevel = -1;
    bool lockingEnabled;
    int lockedRisingEdgeIndex = -1;

    OscilloscopeSettings oscSettings; // Oscilloscope settings
    TriggerType currentTriggerType = NoTrigger;
    WaveformData waveformData;
    WaveformData lockedWaveformData;
    WaveformData currentBuffer;
    WaveformData sampledData;
    bool isSampling = false;

    double triggerLevel = 0.0;


    //bool isTrig1Set = false;
    //bool isTrig2Set = false;

    bool isTrig1Hit = false;
    bool isTrig2Hit = false;

    double zoomLevel = 30.0;

    int shiftValue = 0;

//    void setupOscilloscopeControls();
    void applyTriggerSettings();

    bool snapShot = false;
    WaveformData snapShotData;
    void  analyzeWaveformData();
    void  onDataSliderInit();
    int timerId;
    void smoothing();
    double calculateWaveformSmoothness();
    void smoothWaveformData(double windowSize);
private slots:
    void onAutoSmoothChanged(int state);
    void onBrowseFile();
    QString isConnected();

    void setRisingEdgeTrigger();
    void setFallingEdgeTrigger();
    void setTriggerLevel();

    void drawWaveform(QLabel* label, const QVector<double>& data);
    void generateWaveformData();
    void onDataSliderChanged();
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
    void initDMA();
//    void updateTimerInterval();
protected:
//    void timerEvent(QTimerEvent *event) override;
};

#endif // MAINWINDOW_H
