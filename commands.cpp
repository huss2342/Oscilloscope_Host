//******** commands.cpp
#include "commands.h"
#include <QBigEndianStorageType>

// Helper function to convert uint32_t to big Endian QByteArray
QByteArray uint32ToBigEndian(uint32_t value) {
    QByteArray byteArray;
    byteArray.append(static_cast<char>((value >> 24) & 0xFF));
    byteArray.append(static_cast<char>((value >> 16) & 0xFF));
    byteArray.append(static_cast<char>((value >> 8) & 0xFF));
    byteArray.append(static_cast<char>(value & 0xFF));
    return byteArray;
}


Poke::Poke(QSerialPort *serial, QObject *parent) : QObject(parent), serial(serial) {}

void Poke::execute(uint32_t address, uint32_t data) {
    QByteArray message;
    message.append('W'); // Command character

    // Convert address and data to big Endian and append
    message.append(uint32ToBigEndian(address));
    message.append(uint32ToBigEndian(data));

    // Write the message to the serial port
    serial->write(message);
    serial->flush();
}

Peek::Peek(QSerialPort *serial, QObject *parent) : QObject(parent), serial(serial) {}

QByteArray Peek::execute(uint32_t address) {
    QByteArray message;
    message.append('R'); // Command character
    message.append(uint32ToBigEndian(address)); // Convert address to big Endian

    serial->write(message);
    serial->flush();
    if (serial->waitForReadyRead(1000)) {
        QByteArray responseData = serial->readAll();
        if (responseData.size() >= sizeof(uint32_t)) {
            // Convert from big endian to host endianess
            uint32_t responseValue = qFromBigEndian<quint32>(
                reinterpret_cast<const uchar*>(responseData.constData()));
            return QByteArray(reinterpret_cast<const char*>(&responseValue), sizeof(responseValue));
        }
    }
    return QByteArray(); // Return empty if no response
}



Version::Version(QSerialPort *serial, QObject *parent) : QObject(parent), serial(serial) {}

QByteArray Version::execute() {
    QByteArray message;
    message.append('V'); // Command character

    serial->write(message);
    serial->flush();
    if (serial->waitForReadyRead(1000)) {
        return serial->readAll();
    }
    return QByteArray(); // Return empty if no response
}
