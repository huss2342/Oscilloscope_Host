//******** firmwareupdater.cpp
#include "FirmwareUpdater.h"
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QThread>
#include <sstream>
#include <QDateTime>

FirmwareUpdater::FirmwareUpdater(const QString &comPortName, const QString &firmwarePath, QObject *parent)
    : QObject(parent), portName(comPortName), firmwarePath(firmwarePath), baudRate(921600) {}

int FirmwareUpdater::getLineCount(QTextStream &in) {
    QString line;
    int lineCount = 0;
    while (!in.atEnd()) {
        line = in.readLine();
        lineCount++;
    }

    return --lineCount; // Remove the first line
}

void FirmwareUpdater::resetLine(QTextStream &in) {
    in.seek(0); // Reset the position of QTextStream to the start
    in.readLine(); // Skip the first line
}


void FirmwareUpdater::bytesExtractor(QTextStream &in, std::vector<unsigned char> &programBytes) {
    QString line;
    while (!in.atEnd()) {
        line = in.readLine();
        QStringList parts = line.split('\t');
        if (parts.size() > 1) {
            bool ok;
            unsigned long long secondPartInt = parts[1].toULongLong(&ok, 16);
            if (ok) {
                programBytes.push_back((secondPartInt & 0xFF000000) >> 24);
                programBytes.push_back((secondPartInt & 0xFF0000) >> 16);
                programBytes.push_back((secondPartInt & 0xFF00) >> 8);
                programBytes.push_back((secondPartInt & 0xFF));
            }
        }
    }
}

bool FirmwareUpdater::initializeSerialCommunication(QSerialPort &serial) {
    serial.setPortName(portName);
    serial.setBaudRate(baudRate);

    if (!serial.open(QIODevice::ReadWrite)) {
        emit updateStatus("ERROR: cannot open serial port: " + portName + ". " + serial.errorString() );
        return false;
    }
    emit updateStatus("Serial port opened successfully: " + portName );

    return true;
}


bool FirmwareUpdater::transmitFirmwareData(QSerialPort &serial, int lineCount, const std::vector<unsigned char> &programBytes) {

    // 1 ----------------------------- send the character 'P' to the microcontroller ----------------------------
    QByteArray dataToSend(1, 'P'); // Create a QByteArray with a single character 'P'
    qint64 bytesWritten = serial.write(dataToSend);
    if (bytesWritten == -1) {
        emit updateStatus("Error: Failed to send 'P'");
        return false;
    }

    // Wait until all bytes are written or timeout occurs
    if (!serial.waitForBytesWritten(1000)) { // Timeout in milliseconds
        emit updateStatus("Error: Timeout occurred while sending 'P'");
        return false;
    }

    emit updateStatus("Sent the character 'P'. Bytes Sent: " + QString::number(bytesWritten));


    // 2 ----------------------------- convert the lineCount to big endian and send ----------------------------
    lineCount = lineCount * 4;
    uint32_t lineCountBigEndian = ((lineCount & 0xff000000) >> 24) |
                                  ((lineCount & 0x00ff0000) >> 8) |
                                  ((lineCount & 0x0000ff00) << 8) |
                                  ((lineCount & 0x000000ff) << 24);
    QByteArray lineCountArray(reinterpret_cast<const char *>(&lineCountBigEndian), sizeof(lineCountBigEndian));

    // Debug print the contents of lineCountArray
        //    QString lineCountDebugString;
        //    for (int i = 0; i < lineCountArray.size(); ++i) {
        //        lineCountDebugString += QString::number(static_cast<uint8_t>(lineCountArray[i]), 16).rightJustified(2, '0').toUpper() + " ";
        //    }
        //    emit updateStatus("Contents of lineCountArray: " + lineCountDebugString);

    bytesWritten = serial.write(lineCountArray);

    if (bytesWritten == -1) {
        emit updateStatus("Error: Failed to send line count");
        return false;
    }

    // Wait until all bytes are written or timeout occurs
    if (!serial.waitForBytesWritten(1000)) { // Timeout in milliseconds
        emit updateStatus("Error: Timeout occurred while sending 'P'");
        return false;
    }

    emit updateStatus("Line count (" + QString::number(lineCount/4) + ") sent successfully. Bytes Sent: " + QString::number(bytesWritten));

    // 3 ----------------------------- send the bytes to the microcontroller ----------------------------
    QByteArray firmwareByteArray(reinterpret_cast<const char *>(&programBytes[0]), static_cast<int>(programBytes.size()));

    // Debug print the contents of firmwareByteArray
        //    QString firmwareDebugString;
        //    for (int i = 0; i < firmwareByteArray.size(); ++i) {
        //        firmwareDebugString += QString::number(static_cast<uint8_t>(firmwareByteArray[i]), 16).rightJustified(2, '0').toUpper() + " ";
        //    }
        //    emit updateStatus("Contents of firmwareByteArray: " + firmwareDebugString);

    bytesWritten = serial.write(firmwareByteArray);

    if (bytesWritten == -1) {
        emit updateStatus("Error: Failed to send firmware data");
        return false;
    }

    // Wait until all bytes are written or timeout occurs
    if (!serial.waitForBytesWritten(1000)) { // Timeout in milliseconds
        emit updateStatus("Error: Timeout occurred while sending 'P'");
        return false;
    }

    emit updateStatus("Firmware data sent successfully. Bytes Sent: " + QString::number(bytesWritten));


    // close the serial port
//    serial.close();
    emit updateStatus("--------- --------- ---------");

    return true;
}



bool FirmwareUpdater::updateFirmware(QSerialPort &serial) {
    emit updateStatus("--------- " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + " ---------");

    QFile firmwareFile(firmwarePath);
    if (!firmwareFile.open(QIODevice::ReadOnly)) {
        emit updateStatus("Error opening file");
        return false;
    }

    QTextStream in(&firmwareFile);
    int lineCount = getLineCount(in);

    resetLine(in);

    std::vector<unsigned char> programBytes;
    bytesExtractor(in, programBytes);
    firmwareFile.close();

//    QSerialPort serial;
//    if (!initializeSerialCommunication(serial)) {
//        return false;
//    }

    return transmitFirmwareData(serial, lineCount, programBytes);
}

