//******** mainwindow.cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "commands.h"
#include <QTimer>
#include <firmwareupdater.h>
#include <QFileDialog>
#include <QSerialPortInfo>
#include <QRegularExpression>
#include <QValidator>


/*
921600
------
---- spi
ffffffc4
0030dad0
*/

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isSampling(false)
    , m_triggerLevel(0.0)
    , m_gain(1.0)
    , m_dataMultiplier(1.0)
    , m_isTrig1Hit(false)
    , m_isTrig2Hit(false)
    , m_zoomLevel(30)
    , m_snapShot(false)
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
    connect(ui->snapshotButton, &QPushButton::clicked, this, &MainWindow::onSnapshot);

    // start sampling
    connect(ui->startSampling, &QPushButton::clicked, this, &MainWindow::onStartStopSampling);

    // init DMA
    connect(ui->InitDMA, &QPushButton::clicked, this, &MainWindow::initDMA);

    // Create and connect the WaveformThread
    m_waveformThread = new WaveformThread(this);
    connect(m_waveformThread, &WaveformThread::waveformDrawn, this, &MainWindow::onWaveformDrawn);
}

void MainWindow::onWaveformDrawn(){
    // do anything else after the  waveforms are drawn.
    // just incase i want to do the calculations here and
    // leave the drawing only to be on the thread maybe?
}

void MainWindow::onStartStopSampling() {
    if (isConnected().isEmpty()) {
        logInfo("Error: There is no comm port connection");
        return;
    }
    if (!m_isSampling) {
        m_isSampling = true;
        logInfo("..... MEASURING .....");
        ui->startSampling->setText("Stop Sampling");
        m_sampledData.channel1.clear();
        m_sampledData.channel2.clear();
        connect(&serial, &QSerialPort::readyRead, this, &MainWindow::Sampling);
        QTimer::singleShot(0, this, &MainWindow::sampleAndUpdateWaveforms);

        // GO BIT
        QString addressStr = "FFFFFFB0";
        QString dataStr = "1";
        bool isHex = true;
        bool debug = false;
        onPoke(addressStr, dataStr, isHex, debug);

    } else {
        m_isSampling = false;
        ui->startSampling->setText("Start Sampling");
        logInfo("..... STOPPING .....");

        // GO BIT
        QString addressStr = "FFFFFFB0";
        QString dataStr = "0";
        bool isHex = true;
        bool debug = false;
        onPoke(addressStr, dataStr, isHex, debug);

        // Read and discard all available data in the serial buffer
//        while (serial.bytesAvailable() > 0) {
//            serial.readAll();
//        }

        disconnect(&serial, &QSerialPort::readyRead, this, &MainWindow::Sampling);
    }
}

void MainWindow::sampleAndUpdateWaveforms() {
    if (m_isSampling) {
        Sampling();
        updateWaveforms();
        QTimer::singleShot(0, this, &MainWindow::sampleAndUpdateWaveforms);
    }
}
// --------------------------------------------- Graphing

void MainWindow::Sampling() {
    // 8 bits datain
    QByteArray data = serial.readAll();

    for (const auto &byte : data) {
        quint8 unsignedValue = static_cast<quint8>(byte);
        int8_t signedValue = static_cast<int8_t>(unsignedValue);

        double value = static_cast<double>(signedValue);
        m_sampledData.channel1.append(value);

        // debugging
        if(unsignedValue < 0 || signedValue < 0 || value < 0){
            qDebug() << "usVal: " << unsignedValue << "sVal" << signedValue  << "val" << value;
        }

        // Remove the oldest sample if the buffer size exceeds 1024
        if (m_sampledData.channel1.size() > 512 * 10) {
            m_sampledData.channel1.removeFirst();
        }
    }

    // Update the current buffer with the most recent 512 samples
    if (m_sampledData.channel1.size() >= 512) {
        m_currentBuffer.channel1 = m_sampledData.channel1.mid(m_sampledData.channel1.size() - 512);
    }
}

void MainWindow::updateWaveforms() {
    generateWaveformData();
    m_waveformThread->setWaveformData(m_waveformData);
    m_waveformThread->setLabels(ui->sineWaveLabel, ui->squareWaveLabel);
    m_waveformThread->setOscilloscopeSettings(m_oscSettings);
    m_waveformThread->setZoomLevel(m_zoomLevel);
    m_waveformThread->start();
}


void MainWindow::generateWaveformData() {
    if (!m_snapShot) {
        if (!m_isTrig1Hit) {
            m_waveformData.channel1.clear();
            if (!m_currentBuffer.channel1.isEmpty()) {
                m_waveformData.channel1 = m_currentBuffer.channel1;
            }
        } else {
            m_waveformData.channel1 = m_snapShotData.channel1;
        }

        if (!m_isTrig2Hit) {
            m_waveformData.channel2.clear();
            for (int i = 0; i < 511; ++i) {
                m_waveformData.channel2.append(((i % 20) < 10 ? 1 : -1) * m_dataMultiplier); // Apply multiplier
            }
        } else {
            m_waveformData.channel2 = m_snapShotData.channel2;
        }
    } else {
        m_waveformData.channel1 = m_snapShotData.channel1;
        m_waveformData.channel2 = m_snapShotData.channel2;
    }
}

void MainWindow::onSnapshot() {
    if (!m_snapShot) {
        m_snapShotData.channel1 = m_waveformData.channel1;
        m_snapShotData.channel2 = m_waveformData.channel2;
        logInfo("SNAPSHOT ENABLED");
        ui->triggerButton->setEnabled(false);
        m_snapShot = true;
    } else {
        logInfo("SNAPSHOT DISABLED");
        ui->triggerButton->setEnabled(true);
        m_snapShot = false;
    }


    // Update the waveforms
    updateWaveforms();
}

void MainWindow::setRisingEdgeTrigger() {
    if (m_oscSettings.triggerType == RisingEdge) {
        m_oscSettings.triggerType = NoTrigger; // Toggle off if already set
        logInfo("Rising edge deselected");
    } else {
        m_oscSettings.triggerType = RisingEdge;
        logInfo("Rising edge selected");
    }
    updateWaveforms();
}

void MainWindow::setFallingEdgeTrigger() {
    if (m_oscSettings.triggerType == FallingEdge) {
        m_oscSettings.triggerType = NoTrigger; // Toggle off if already set
        logInfo("Falling edge deselected");
    } else {
        m_oscSettings.triggerType = FallingEdge;
        logInfo("Falling edge selected");
    }
    updateWaveforms();
}

void MainWindow::setTriggerLevel() {
    bool ok;
    double level = ui->triggerValue->toPlainText().toDouble(&ok);

    if(m_oscSettings.triggerType == TriggerLevel){
        m_oscSettings.triggerType = NoTrigger; // Toggle off if already set
        logInfo("Trigger deselected");
        m_isTrig1Hit = false;
        m_isTrig2Hit = false;
        updateWaveforms();

    } else {

        if (!ok) {
            ui->triggerValue->clear();
            logInfo("ERROR: Invalid input. Please enter a valid number.");
            return;
        }

        m_oscSettings.triggerType = TriggerLevel;
        m_oscSettings.triggerLevel = level;
        logInfo("Trigger level set to: " + QString::number(level));

        updateWaveforms();
    }
}


void MainWindow::setupOscilloscopeControls() {
    // Initialize gain based on the dial's initial value
    m_gain = ui->dial->value() / 100.0;
    ui->dialValueLabel->setText(QString::number(ui->dial->value()));

    // Connect the dial for gain adjustments
    connect(ui->dial, &QDial::valueChanged, this, [this](int value) {
        m_gain = value / 100.0;
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


void MainWindow::onDataSliderChanged(double value) {
    m_dataMultiplier = value;

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

    if (m_isSampling) {
        logInfo("Error: Cannot peek while sampling is active");
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

void MainWindow::initDMA(){
    bool isHex = true;
    bool debug = false;

    // STARTING ADDRESS
    QString addressStr = "FFFFFFBC";
    QString dataStr = "FFFFFFE4";
    onPoke(addressStr, dataStr, isHex, debug);

    // LENGTH
    addressStr = "FFFFFFB8";
    dataStr = "1";
    onPoke(addressStr, dataStr, isHex, debug);


}

void MainWindow::turnOnBoard() {
    bool isHex = true;
    bool debug = false;
    QString dataStr = "ffffffff";

    QString addressStr = "fffffff4";
    onPoke(addressStr, dataStr, isHex, debug);


    addressStr = "fffffff0";
    onPoke(addressStr, dataStr, isHex, debug);

    logInfo("Board turned on");
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
        serial.setBaudRate(921600);
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
    m_zoomLevel *= 1.1; // Decrease the zoom level by 10%
    updateWaveforms();
}

void MainWindow::onDefaultZoom() {
    m_zoomLevel = 1.0; // Reset the zoom level to default
    updateWaveforms();
}

void MainWindow::onZoomIn() {
    m_zoomLevel *= 0.9; // Increase the zoom level by 10%
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
    // Stop and delete the WaveformThread
    if (m_waveformThread) {
        m_waveformThread->requestInterruption(); // Request the thread to stop
        m_waveformThread->wait(); // Wait for the thread to finish
        delete m_waveformThread;
        m_waveformThread = nullptr;
    }

    delete ui;
}
