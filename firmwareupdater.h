//******** firmwareupdater.h
#ifndef FIRMWAREUPDATER_H
#define FIRMWAREUPDATER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <vector>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

class FirmwareUpdater : public QObject {
    Q_OBJECT

public:
    FirmwareUpdater(const QString &comPortName, const QString &firmwarePath, QObject *parent = nullptr);
    bool updateFirmware(QSerialPort &serial);

signals:
    void updateStatus(const QString &status);

private:
    QString portName;
    QString firmwarePath;
    int baudRate;

    void bytesExtractor(QTextStream &in, std::vector<unsigned char> &programBytes);
    void resetLine(QTextStream &in);
    int getLineCount(QTextStream &in);
    bool initializeSerialCommunication(QSerialPort &serial);
    bool transmitFirmwareData(QSerialPort &serial, int lineCount, const std::vector<unsigned char> &programBytes);
};

#endif // FIRMWAREUPDATER_H
