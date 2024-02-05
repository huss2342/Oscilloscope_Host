#ifndef COMMANDS_H
#define COMMANDS_H


#include <QObject>
#include <QSerialPort>

class Poke : public QObject {
    Q_OBJECT
public:
    explicit Poke(QSerialPort *serial, QObject *parent = nullptr);
    void execute(uint32_t address, uint32_t data);

private:
    QSerialPort *serial;
};

class Peek : public QObject {
    Q_OBJECT
public:
    explicit Peek(QSerialPort *serial, QObject *parent = nullptr);
    QByteArray execute(uint32_t address);

private:
    QSerialPort *serial;
};

class Version : public QObject {
    Q_OBJECT
public:
    explicit Version(QSerialPort *serial, QObject *parent = nullptr);
    QByteArray execute();

private:
    QSerialPort *serial;
};

#endif // COMMANDS_H
