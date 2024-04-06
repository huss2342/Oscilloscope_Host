//******** mainwindow.cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "commands.h"
#include <QTimer>
#include <QPainter>
#include <firmwareupdater.h>
#include <cmath>
#include <QFileDialog>
#include <QSerialPortInfo>
#include <QRegularExpression>
#include <QValidator>
#include <QPainterPath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // ----------------------------------------- LHS -----------------------------------------

    // first row
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::initializeSerialCommunication);
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshCOMPorts);

    // fill in the com combobox
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        // Check if the description contains "Standard Serial over Bluetooth link"
        if (!port.description().contains("Standard Serial over Bluetooth link", Qt::CaseInsensitive)) {
            ui->comPortComboBox->addItem(port.portName());
        }
    }

    // second row
    connect(ui->browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseFile);
    connect(ui->updateButton, &QPushButton::clicked, this, &MainWindow::onUpdateFirmware);

    // third row
    QRegularExpression peekHexRegExp("^[0-9A-Fa-f]{0,8}$");
    ui->peekAddressEdit->setValidator(new QRegularExpressionValidator(peekHexRegExp, this));
    connect(ui->peekButton, &QPushButton::clicked, this, [this]() {
        onPeek(ui->peekAddressEdit->text());
    });

    // fourth row
    // Define the lambda function to update maxLength of pokeDataEdit based on the radio button states
    auto updateDataEditMaxLengthAndValidator = [this]() {
        static QRegularExpression hexRegExp("^[0-9A-Fa-f]{0,8}$");
        static QRegularExpression decRegExp("^[0-9]{0,10}$");
        QValidator* validator = nullptr;
        if(ui->hexRadioButton->isChecked()) {
            ui->pokeDataEdit->setMaxLength(8);
            validator = new QRegularExpressionValidator(hexRegExp, this);
        } else if (ui->decRadioButton->isChecked()) {
            ui->pokeDataEdit->setMaxLength(10);
            validator = new QRegularExpressionValidator(decRegExp, this);
            // Check if current text in pokeDataEdit is not valid decimal
            if(!decRegExp.match(ui->pokeDataEdit->text()).hasMatch()) {
                ui->pokeDataEdit->clear(); // Clear if it contains hex characters
            }
        }
        ui->pokeDataEdit->setValidator(validator);
    };


    // Connect the toggled signal of the radio buttons to the lambda function
    connect(ui->hexRadioButton, &QRadioButton::toggled, this, updateDataEditMaxLengthAndValidator);
    connect(ui->decRadioButton, &QRadioButton::toggled, this, updateDataEditMaxLengthAndValidator);

    // Connect the clicked signal of the poke button to perform the poke operation with current settings
    connect(ui->pokeButton, &QPushButton::clicked, this, [this]() {
        onPoke(ui->pokeAddressEdit->text(), ui->pokeDataEdit->text(), ui->hexRadioButton->isChecked());
    });

    // call the function so that it initializes
    updateDataEditMaxLengthAndValidator();


    // fifth row
    connect(ui->version, &QPushButton::clicked, this, &MainWindow::onVersion);
    connect(ui->clearLog, &QPushButton::clicked, this, &MainWindow::onClear);
    connect(ui->turnonButton, &QPushButton::clicked, this, &MainWindow::turnOnBoard);

    // sixth row
    connect(ui->zoomoutButton, &QPushButton::clicked, this, &MainWindow::onZoomOut);
    connect(ui->defaultZoomButton, &QPushButton::clicked, this, &MainWindow::onDefaultZoom);
    connect(ui->zoominButton, &QPushButton::clicked, this, &MainWindow::onZoomIn);

    // ----------------------------------------- RHS -----------------------------------------
    setupOscilloscopeControls();

    connect(ui->dataSlider, &QSpinBox::valueChanged, this, &MainWindow::onDataSliderChanged);

    // Other connections for trigger settings
    connect(ui->risingEdgeButton, &QPushButton::clicked, this, &MainWindow::setRisingEdgeTrigger);
    connect(ui->fallingEdgeButton, &QPushButton::clicked, this, &MainWindow::setFallingEdgeTrigger);
    connect(ui->triggerButton, &QPushButton::clicked, this, &MainWindow::setTriggerLevel);


}

// --------------------------------------------- Graphing

void MainWindow::updateWaveforms() {
    generateWaveformData(); // creates the waves. replace with checking  in the port !!!!!!!!!!!!!!!!!!!!!!!!!!
    analyzeWaveformData(); // Call after generating data to analyze for trigger levels and log information
    drawWaveform(ui->sineWaveLabel, waveformData.channel1);
    drawWaveform(ui->squareWaveLabel, waveformData.channel2);
}

void MainWindow::drawWaveform(QLabel* label, const QVector<double>& data) {
    if (data.isEmpty()) return;

    // Setup for drawing
    QSize labelSize = label->size();
    QPixmap pixmap(labelSize);
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    QPen pen(Qt::black);
    painter.setPen(pen);

    double xScale = labelSize.width() / static_cast<double>(data.size() - 1);
    double yScale = (labelSize.height() / 2.0) * gain / zoomLevel;

    QPainterPath path;
    double yPos = labelSize.height() / 2.0 + data[0] * yScale;
    path.moveTo(0, yPos);

    for (int i = 1; i < data.size(); ++i) {
        double nextYPos = labelSize.height() / 2.0 + data[i] * yScale;
        double xPos = i * xScale;
        path.lineTo(xPos, nextYPos);

        // Rising Edge detection and highlighting
        if (oscSettings.triggerType == RisingEdge && data[i] < data[i-1]) {
            painter.setPen(QPen(Qt::red, 2));
            painter.drawEllipse(QPointF(xPos, nextYPos), 2, 2);
            painter.setPen(pen);
        }

        // Falling Edge detection and highlighting
        if (oscSettings.triggerType == FallingEdge && data[i] > data[i-1]) {
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

    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPath(path);
    label->setPixmap(pixmap);
}

void MainWindow::generateWaveformData() {
    waveformData.channel1.clear();
    waveformData.channel2.clear();
    for (int i = 0; i < 511; ++i) {
        waveformData.channel1.append(qSin(i * 0.1 ) * dataMultiplier); // Apply multiplier
        waveformData.channel2.append(((i % 20) < 10 ? 1 : -1) * dataMultiplier); // Apply multiplier
    }
}



void MainWindow::setRisingEdgeTrigger() {
    if (oscSettings.triggerType == RisingEdge) {
        oscSettings.triggerType = NoTrigger; // Toggle off if already set
        logInfo("Rising edge trigger deselected");
    } else {
        oscSettings.triggerType = RisingEdge;
        logInfo("Rising edge trigger selected");
    }
    updateWaveforms();
}

void MainWindow::setFallingEdgeTrigger() {
    if (oscSettings.triggerType == FallingEdge) {
        oscSettings.triggerType = NoTrigger; // Toggle off if already set
        logInfo("Falling edge trigger deselected");
    } else {
        oscSettings.triggerType = FallingEdge;
        logInfo("Falling edge trigger selected");
    }
    updateWaveforms();
}

void MainWindow::setTriggerLevel() {
    bool ok;
    double level = ui->triggerValue->toPlainText().toDouble(&ok);

    if (!ok) {
        ui->triggerValue->clear();
        logInfo("ERROR: Invalid input. Please enter a valid number.");
        return;
    }

    oscSettings.triggerType = TriggerLevel;
    oscSettings.triggerLevel = level;
    logInfo("Trigger level set to: " + QString::number(level));
    triggerSet = -2;

    updateWaveforms();
}


void MainWindow::setupOscilloscopeControls() {
    // Initialize gain based on the dial's initial value
    gain = ui->dial->value() / 100.0;
    ui->dialValueLabel->setText(QString::number(ui->dial->value()));

    // Connect the dial for gain adjustments
    connect(ui->dial, &QDial::valueChanged, this, [this](int value) {
        gain = value / 100.0;
        ui->dialValueLabel->setText(QString::number(value));
        updateWaveforms();
    });

    // data slider
    connect(ui->dataSlider, &QSpinBox::valueChanged, this, [this]() {
        generateWaveformData();
        updateWaveforms();
    });
    ui->dataSlider->setValue(0);


    // Initial call to updateWaveforms to draw waveforms based on initial settings
    QTimer::singleShot(0, this, &MainWindow::updateWaveforms);
}

void MainWindow::analyzeWaveformData() {
    bool triggerLevelReachedChannel1 = std::any_of(waveformData.channel1.begin(), waveformData.channel1.end(), [this](double value) {
        return oscSettings.triggerType == TriggerLevel && std::abs(value) >= oscSettings.triggerLevel;
    });

    bool triggerLevelReachedChannel2 = std::any_of(waveformData.channel2.begin(), waveformData.channel2.end(), [this](double value) {
        return oscSettings.triggerType == TriggerLevel && std::abs(value) >= oscSettings.triggerLevel;
    });

    if (triggerLevelReachedChannel1 && triggerSet == -2) {
        logInfo("Trigger Level reached: Wave 1 (sine), trigger level: " + QString::number(oscSettings.triggerLevel));
        ++triggerSet;
    }
    if (triggerLevelReachedChannel2 && triggerSet == -1) {
        logInfo("Trigger Level reached: Wave 2 (square), trigger level: " + QString::number(oscSettings.triggerLevel));
        ++triggerSet;
    }
}

void MainWindow::onDataSliderChanged(double value) {
    dataMultiplier = value;

    // Regenerate and redraw waveforms with the new multiplier
    updateWaveforms();
}

// --------------------------------------------- PEEK, POKE AND VERSION

void MainWindow::onPoke(const QString &addressStr, const QString &dataStr, bool isHex, bool debug) {
    if (isConnected().isEmpty()) {
        logInfo("Error: There is no comm port connection");
        return;
    }

    bool okAddress, okData;
    uint32_t address = addressStr.toUInt(&okAddress, 16); // Address is always hex
    if(!okAddress){
        logInfo("invalid address format");
    }

    uint32_t data;

    if (isHex) {
        data = dataStr.toUInt(&okData, 16);
    } else {
        data = dataStr.toUInt(&okData);
        // Check if data exceeds 32 bits after conversion to hex
        if (okData && QString::number(data, 16).length() > 8) {
            logInfo("Data exceeds 32 bits");
            return;
        }
    }

    if (!okData) {
        logInfo("Invalid Data format");
        return;
    }


    Poke poke(&serial);
    poke.execute(address, data);

    // Log message in both decimal and hex
    QString decimalData = QString::number(data);
    QString hexData = QString::number(data, 16).toUpper();
    if (debug){
        QString messageLog = QString("Sent: Address = 0x%1, Data = %2 (0x%3)").arg(addressStr, decimalData, hexData);
        logInfo(messageLog);
    }
}

void MainWindow::onPeek(const QString &addressStr, bool debug) {
    if (isConnected().isEmpty()) {
        logInfo("Error: There is no comm port connection");
        return;
    }

    bool ok;
    uint64_t longAddress = addressStr.toULongLong(&ok, 16);
    uint32_t address = static_cast<uint32_t>(longAddress);
    if (!ok) {
        logInfo("Invalid address format");
        return;
    }

    Peek peek(&serial);
    QByteArray responseData = peek.execute(address);

    // Process and display response data
    if (!responseData.isEmpty() && responseData.size() >= sizeof(uint32_t)) {
        if(debug){
            uint32_t responseValue;
            memcpy(&responseValue, responseData.data(), sizeof(uint32_t));
            logInfo("Response: " + QString::number(responseValue) + " (0x" + QString::number(responseValue, 16).toUpper() + ")");
        }
    } else {
        logInfo("Invalid response received");
    }

}

void MainWindow::onVersion() {
    if (isConnected().isEmpty()) {
        logInfo("Error: There is no comm port connection");
        return;
    }

    Version version(&serial);
    QByteArray response = version.execute();

    if (!response.isEmpty()) {
        logInfo("Version: " + QString(response[0]));
    } else {
        logInfo("No response received for version request");
    }
}

/*
fffffff4 ffffffff
fffffff0 ffffffff
------
length reg
FFFFFFB8  200000
2097152

starting address reg
FFFFFFBC

go bit reg
FFFFFFB0

---- spi
ffffffc4
0030dad0
*/

void MainWindow::turnOnBoard() {
    bool isHex = true;
    bool debug = false;
    QString dataStr = "ffffffff";

    QString addressStr = "fffffff4";
    onPoke(addressStr, dataStr, isHex, debug);


    addressStr = "fffffff0";
    onPoke(addressStr, dataStr, isHex, debug);
}

// --------------------------------------------- FIRMWARE

void MainWindow::onBrowseFile() {
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Firmware File"), "",
                                                    tr("Firmware Files (*.bin);;All Files (*)"));

    if (!fileName.isEmpty()) {
        ui->firmwarePathEdit->setText(fileName);
    }
}

void MainWindow::onRefreshCOMPorts() {
    ui->comPortComboBox->clear(); // Clear existing items
    logInfo("refreshed");

    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        // Assuming you want to exclude Bluetooth ports from the list
        if (!port.description().contains("Standard Serial over Bluetooth link", Qt::CaseInsensitive)) {
            ui->comPortComboBox->addItem(port.portName()); // Use ui-> to access comPortComboBox
        }
    }
}

void MainWindow::onUpdateFirmware() {
    QString firmwarePath = ui->firmwarePathEdit->text();
    QString comPort = ui->comPortComboBox->currentText();
    FirmwareUpdater *updater = new FirmwareUpdater(comPort, firmwarePath, this);

    connect(updater, &FirmwareUpdater::updateStatus, this, &MainWindow::updateStatusLabel);
    updater->updateFirmware(serial);
}

// --------------------------------------------- LOG

void MainWindow::updateStatusLabel(const QString &status) {
    ui->logConsole->appendPlainText(status);

}

void MainWindow::logInfo(const QString &message) {
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm");
    ui->logConsole->appendPlainText(timestamp + ": " + message);
}

void MainWindow::onClear(){
    //clears log
    ui->logConsole->clear();
}

// --------------------------------------------- COMM

void MainWindow::initializeSerialCommunication() {
    if (serial.isOpen()) {
        serial.close();
        ui->connectButton->setText("Connect");
        ui->connectButton->setStyleSheet("color: red; background-color: white;");
        logInfo("Disconnected from "+ serial.portName());
    } else {
        serial.setPortName(ui->comPortComboBox->currentText()); // Ensure this is correct
        serial.setBaudRate(QSerialPort::Baud115200);
        if (serial.open(QIODevice::ReadWrite)) {
            ui->connectButton->setText("Disconnect");
            ui->connectButton->setStyleSheet("color: green; background-color: white;");
            logInfo("Connected to " + serial.portName());
        } else {
            logInfo("Failed to open port " + serial.portName());
        }
    }
}


QString MainWindow::isConnected() {
    if (serial.isOpen()) {
        return serial.portName();
    } else {
        return QString();
    }
}

// --------------------------------------------- ZOOM LEVEL MANAGMENT

void MainWindow::onZoomOut() {
    zoomLevel *= 1.1; // Decrease the zoom level by 10%
    updateWaveforms();
}

void MainWindow::onDefaultZoom() {
    zoomLevel = 1.0; // Reset the zoom level to default
    updateWaveforms();
}

void MainWindow::onZoomIn() {
    zoomLevel *= 0.9; // Increase the zoom level by 10%
    updateWaveforms();
}


// ---------------------------------------------
void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event); // Call base class implementation
//    zoomLevel = 1.0;
    updateWaveforms(); // Update the waveforms with the new size
}

MainWindow::~MainWindow()
{
    delete ui;
}
