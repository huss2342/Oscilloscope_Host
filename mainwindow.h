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

    OscilloscopeSettings oscSettings; // Oscilloscope settings
    TriggerType currentTriggerType = NoTrigger;
    WaveformData waveformData;
    double triggerLevel = 0.0;

    double gain = 1.0;
    double dataMultiplier;
    int triggerSet = -2;

    void setupOscilloscopeControls();
    void applyTriggerSettings();
private slots:
    void onBrowseFile();
    QString isConnected();

    void setRisingEdgeTrigger();
    void setFallingEdgeTrigger();
    void setTriggerLevel();

    void drawWaveform(QLabel* label, const QVector<double>& data);
    void generateWaveformData();
    void analyzeWaveformData();
    void onDataSliderChanged(double value);

    void onClear();
    void onPeek(const QString &addressStr);
    void onPoke(const QString &addressStr, const QString &dataStr, bool isHex);
    void onVersion();
    void onRefreshCOMPorts();
    void updateWaveforms();
    void onUpdateFirmware();  // Slot to handle firmware update button click
    void updateStatusLabel(const QString &status);  // Slot to update the status label
    void logInfo(const QString &message);

protected:
    void resizeEvent(QResizeEvent* event) override;
};

#endif // MAINWINDOW_H
