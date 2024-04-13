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
//#include <QElapsedTimer>
//#include <QTimerEvent>
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QBuffer>
#include <QtMath>
#include <complex>
#include <cmath>

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
        onPeek(ui->peekAddressEdit->text(), true);
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
        onPoke(ui->pokeAddressEdit->text(), ui->pokeDataEdit->text(), ui->hexRadioButton->isChecked(), true);
    });

    // call the function so that it initializes
    updateDataEditMaxLengthAndValidator();


    // fifth row
    connect(ui->version, &QPushButton::clicked, this, &MainWindow::onVersion);
    connect(ui->clearLog, &QPushButton::clicked, this, &MainWindow::onClear);
    connect(ui->turnonButton, &QPushButton::clicked, this, &MainWindow::turnOnBoard);
    connect(ui->turnOffButton, &QPushButton::clicked, this, &MainWindow::turnOffBoard);

    // sixth row
    connect(ui->zoomoutButton, &QPushButton::clicked, this, &MainWindow::onZoomOut);
    connect(ui->defaultZoomButton, &QPushButton::clicked, this, &MainWindow::onDefaultZoom);
    connect(ui->zoominButton, &QPushButton::clicked, this, &MainWindow::onZoomIn);

    // ----------------------------------------- RHS -----------------------------------------
    connect(ui->dataSlider, &QSpinBox::valueChanged, this, &MainWindow::onDataSliderChanged);

    // Other connections for trigger settings
    connect(ui->risingEdgeButton, &QPushButton::clicked, this, &MainWindow::highlightRisingEdge);
    connect(ui->fallingEdgeButton, &QPushButton::clicked, this, &MainWindow::highlightFallingEdge);
    connect(ui->triggerButton, &QPushButton::clicked, this, &MainWindow::setTriggerLevel);
    connect(ui->snapshotButton, &QPushButton::clicked, this, &MainWindow::onSnapshot);

    // start sampling
    connect(ui->startSampling, &QPushButton::clicked, this, &MainWindow::onStartStopSampling);

    // init DMA
    connect(ui->InitDMA, &QPushButton::clicked, this, &MainWindow::initDMA);

    // update the sampling interval
    //    connect(ui->SamplingIntervalSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::updateTimerInterval);
    //    updateTimerInterval();

    connect(ui->autoSmoothCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onAutoSmoothChanged);


    // update wave form timer
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateWaveforms);
    updateTimer->start(0); // Update every X milliseconds


    ui->lockingCheckBox->isChecked();
    ui->lockingLevelSlider->value();

}

void MainWindow::onAutoSmoothChanged(int state) {
    if (state == Qt::Checked) {
        logInfo("Auto smooth enabled");
        if (isSampling) {
            smoothing();
        }
    } else {
        logInfo("Auto smooth disabled");
        ui->SamplingIntervalSpinBox->setValue(1);
    }
}

void MainWindow::smoothing() {
    if (!isSampling) {
        logInfo("Error: Sampling is not active");
        return;
    }

    double smoothness = calculateWaveformSmoothness();
    if (smoothness > 0) {
        double windowSize = std::max(1.0, 15.0 - smoothness);

        ui->SamplingIntervalSpinBox->setValue(windowSize);

    } else {
        logInfo("Error: Unable to determine the waveform period");
    }
}


void MainWindow::smoothWaveformData(double windowSize) {

    if (waveformData.channel1.isEmpty()) {
        return; // No data to smooth
    }

    QVector<double> smoothedData;
    int halfWindow = windowSize / 2;

    for (int i = 0; i < waveformData.channel1.size(); ++i) {
        double sum = 0;
        int count = 0;

        // Calculate the average of the samples within the window
        for (int j = i - halfWindow; j <= i + halfWindow; ++j) {
            if (j >= 0 && j < waveformData.channel1.size()) {
                sum += waveformData.channel1[j];
                count++;
            }
        }

        double average = sum / count;
        smoothedData.append(average);
    }

    // Replace the original waveform data with the smoothed data
    waveformData.channel1 = smoothedData;
}

double MainWindow::calculateWaveformSmoothness() {
    if (waveformData.channel1.size() < 2) {
        return 0; // Not enough samples
    }

    double sumDifferences = 0;
    int count = 0;

    for (int i = 1; i < waveformData.channel1.size(); ++i) {
        double diff = std::abs(waveformData.channel1[i] - waveformData.channel1[i - 1]);
        sumDifferences += diff;
        count++;
    }

    double avgDifference = sumDifferences / count;

    // Calculate the smoothness factor based on the average difference
    double smoothnessFactor = 1.0 / (1.0 + avgDifference);

    // Adjust the smoothness factor
    double adjustedSmoothness = std::pow(smoothnessFactor, 0.1) * 10;

    return adjustedSmoothness;
}


/////// SPI

void MainWindow::onDataSliderChanged() {
    int value = ui->dataSlider->value();

    // send 0030---0
    // the three dashes are the values

    QString addressStr = "ffffffc4";
    int data = 0x00300000 | (value << 4);
    QString dataStr = QString::number(data, 16); // Convert to hexadecimal string

    bool isHex = true;
    bool debug = false;

    onPoke(addressStr, dataStr, isHex, debug);
}

// DOES NOT  GET USED:::::  CAN REMOVE LATER
void MainWindow::onDataSliderInit() {
    QString addressStr = "ffffffc4";
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
        uint32_t responseValue;
        memcpy(&responseValue, responseData.data(), sizeof(uint32_t));
        logInfo("Response: " + QString::number(responseValue) + " (0x" + QString::number(responseValue, 16).toUpper() + ")");

        int sliderValue = (responseValue & 0xfff0) >> 4;
        ui->dataSlider->setValue(sliderValue);
    } else {
        logInfo("Invalid or empty response received");
    }
}

void MainWindow::onStartStopSampling() {
    if (isConnected().isEmpty()) {
        logInfo("Error: There is no comm port connection");
        return;
    }

    if (!isSampling) {
        isSampling = true;
        logInfo("..... MEASURING .....");
        ui->startSampling->setText("Stop Sampling");
        sampledData.channel1.clear();
        sampledData.channel2.clear();
        shiftValue = ui->shiftGraphSpinner->value();

        connect(&serial, &QSerialPort::readyRead, this, &MainWindow::Sampling);
        //        updateTimerInterval();

        // GO BIT
        QString addressStr = "FFFFFFB0";
        QString dataStr = "1";
        bool isHex = true;
        bool debug = false;
        onPoke(addressStr, dataStr, isHex, debug);
    } else {
        isSampling = false;
        ui->startSampling->setText("Start Sampling");
        logInfo("..... STOPPING .....");

//        if (timerId != -1) {
//            killTimer(timerId);
//        }

        // GO BIT
        QString addressStr = "FFFFFFB0";
        QString dataStr = "0";
        bool isHex = true;
        bool debug = false;
        onPoke(addressStr, dataStr, isHex, debug);

        // Read and discard all available data in the serial buffer
        while (serial.bytesAvailable() > 0) {
            serial.readAll();
        }
        disconnect(&serial, &QSerialPort::readyRead, this, &MainWindow::Sampling);
    }
}
// --------------------------------------------- Graphing

void MainWindow::Sampling() {
    // 8 bits datain
    QByteArray data = serial.readAll();

    // Append the new data to the sampledData buffer
    for (const auto &byte : data) {
        quint8 unsignedValue = static_cast<quint8>(byte);
        int8_t signedValue = static_cast<int8_t>(unsignedValue);
        //double value = static_cast<double>(signedValue);
        sampledData.channel1.append(signedValue);
    }
}




void MainWindow::updateWaveforms() {
    const int bufferSize = ui->sampleSizeSpinner->value();

    // Update the currentBuffer with the most recent samples
    if (sampledData.channel1.size() >= bufferSize) {
        currentBuffer.channel1 = sampledData.channel1.mid(sampledData.channel1.size() - bufferSize);
        sampledData.channel1.remove(0, sampledData.channel1.size() - bufferSize);
    }

    // waveform <= currentBuffer

    if (isSampling && ui->autoSmoothCheckBox->isChecked()) {
        smoothing();
    }
    smoothWaveformData(ui->SamplingIntervalSpinBox->value());
    generateWaveformData(); // creates the waves
    analyzeWaveformData(); // Call after generating data to analyze for trigger levels and log information

    drawWaveform(ui->sineWaveLabel, waveformData.channel1);
    drawWaveform(ui->squareWaveLabel, waveformData.channel2);
}


void MainWindow::drawWaveform(QLabel* label, const QVector<double>& data) {
    if (data.isEmpty()) return;

    const QVector<double>& displayData = data;

    // Setup for drawing
    QSize labelSize = label->size();
    QPixmap pixmap(labelSize);
    pixmap.fill(Qt::white);
    QPainter painter(&pixmap);
    QPen pen(Qt::black);
    painter.setPen(pen);

    double xScale = labelSize.width() / static_cast<double>(displayData.size() - 1);
    double yScale = (labelSize.height() / 2.0) / zoomLevel;

    QPainterPath path;
    int triggerIndex = -1; // Initialize triggerIndex to -1 (no trigger)

    // Check if rising edge trigger is enabled
    if (ui->lockingCheckBox->isChecked()) {
        // Find the index of the first rising edge above the trigger level
        double triggerLevel = ui->lockingLevelSlider->value();
        for (int i = 1; i < displayData.size(); ++i) {
            if (displayData[i] < displayData[i - 1] && displayData[i] > triggerLevel) {
                triggerIndex = i;
                break;
            }
        }
    }

    double yPos;
    int startIndex = 0;
    if (triggerIndex == -1) {
        yPos = labelSize.height() / 2.0 - displayData[0] * yScale - shiftValue;
        path.moveTo(0, yPos);
    } else {
        startIndex = triggerIndex;
        yPos = labelSize.height() / 2.0 - displayData[triggerIndex] * yScale - shiftValue;
        path.moveTo((triggerIndex - 1) * xScale, yPos);
    }

    // Draw a horizontal line at the middle of the screen
    painter.setPen(Qt::darkGreen);
    double midY = labelSize.height() / 2.0;
    painter.drawLine(0, midY, labelSize.width(), midY);
    painter.setPen(pen);

    for (int i = startIndex; i < displayData.size() - 1; ++i) {
        double nextYPos = labelSize.height() / 2.0 - displayData[i] * yScale - shiftValue;
        double xPos = (i - startIndex) * xScale;

        // Rising Edge detection and highlighting
        bool risingEdgeDetected = false;
        if (ui->lockingCheckBox->isChecked() && i > startIndex && displayData[i] < displayData[i - 1] && displayData[i] > ui->lockingLevelSlider->value() && !risingEdgeDetected) {
            painter.setPen(QPen(Qt::magenta, 2));
            painter.drawEllipse(QPointF(xPos, nextYPos), 2, 2);
            painter.setPen(pen);
            risingEdgeDetected = true;
        }

        // Rising Edge detection and highlighting
        if (oscSettings.triggerType == RisingEdgeHighlighter && i < displayData.size() - 1 && displayData[i] < displayData[i + 1]) {
            painter.setPen(QPen(Qt::red, 2));
            painter.drawEllipse(QPointF(xPos, nextYPos), 2, 2);
            painter.setPen(pen);
        }

        // Falling Edge detection and highlighting
        if (oscSettings.triggerType == FallingEdgeHighlighter && i < displayData.size() - 1 && displayData[i] > displayData[i + 1]) {
            painter.setPen(QPen(Qt::blue, 2));
            painter.drawEllipse(QPointF(xPos, nextYPos), 2, 2);
            painter.setPen(pen);
        }

        path.lineTo(xPos, nextYPos);
    }

    // If there are not enough data points after the trigger index, start a new path from the beginning
    if (triggerIndex != -1 && displayData.size() - triggerIndex < labelSize.width() / xScale) {
        QPainterPath remainingPath;
        double remainingXPos = (displayData.size() - triggerIndex) * xScale;

        for (int i = triggerIndex; i < displayData.size(); ++i) {
            double nextYPos = labelSize.height() / 2.0 - displayData[i] * yScale - shiftValue;
            double xPos = (i - triggerIndex) * xScale;

            if (i == triggerIndex) {
                remainingPath.moveTo(xPos, nextYPos);
            } else {
                remainingPath.lineTo(xPos, nextYPos);
            }
        }

        // Draw the remaining path from the beginning only if necessary
        if (remainingXPos < labelSize.width()) {
            for (int i = 0; i < triggerIndex; ++i) {
                double nextYPos = labelSize.height() / 2.0 - displayData[i] * yScale - shiftValue;
                double xPos = remainingXPos + i * xScale;

                if (xPos < labelSize.width()) {
                    remainingPath.lineTo(xPos, nextYPos);
                } else {
                    break;
                }
            }
        }

        painter.drawPath(remainingPath);
    }

    // Calculate max and min values from data
    double maxVal = *std::max_element(displayData.constBegin(), displayData.constEnd());
    double minVal = *std::min_element(displayData.constBegin(), displayData.constEnd());

    // Draw max and min values on the graph
    painter.setPen(Qt::black); // Use black pen for text
    painter.drawText(QPointF(5, 20), QString("Max: %1").arg(maxVal)); // Position these based on your UI layout
    painter.drawText(QPointF(5, labelSize.height() - 5), QString("Min: %1").arg(minVal));

    // Draw trigger line if trigger mode is set
    if (oscSettings.triggerType == TriggerLevel) {
        double triggerYPos = labelSize.height() / 2.0 - oscSettings.triggerLevel * yScale; // Corrected trigger line position
        QPen triggerPen(Qt::green, 1);
        painter.setPen(triggerPen);
        painter.drawLine(0, triggerYPos, labelSize.width(), triggerYPos);
        painter.setPen(pen);
    }

    // Draw locking line if locking is enabled
    double lockingLevel = ui->lockingLevelSlider->value();
    double lockingYPos = labelSize.height() / 2.0 - lockingLevel * yScale;
    QPen lockingPen(Qt::darkBlue, 1);
    if (ui->lockingCheckBox->isChecked()) {
        lockingPen.setColor(Qt::red);
    }
    painter.setPen(lockingPen);
    painter.drawLine(0, lockingYPos, labelSize.width(), lockingYPos);
    painter.setPen(pen);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPath(path);
    label->setPixmap(pixmap);
}


void MainWindow::generateWaveformData() {
    if (!snapShot) {
        if (!isTrig1Hit) {
            waveformData.channel1.clear();
            if (!currentBuffer.channel1.isEmpty()) {
                waveformData.channel1 = currentBuffer.channel1;
            }
        } else {
            waveformData.channel1 = snapShotData.channel1;
        }

        if (!isTrig2Hit) {
            waveformData.channel2.clear();
            //            for (int i = 0; i < 511; ++i) {
            //                waveformData.channel2.append(((i % 20) < 10 ? 1 : -1) * dataMultiplier); // Apply multiplier
            //            }
        } else {
            waveformData.channel2 = snapShotData.channel2;
        }
    } else {
        waveformData.channel1 = snapShotData.channel1;
        waveformData.channel2 = snapShotData.channel2;
    }



}

void MainWindow::onSnapshot() {
    if (!snapShot) {
        snapShotData.channel1 = waveformData.channel1;
        snapShotData.channel2 = waveformData.channel2;
        logInfo("SNAPSHOT ENABLED");
        ui->triggerButton->setEnabled(false);
        snapShot = true;
    } else {
        logInfo("SNAPSHOT DISABLED");
        ui->triggerButton->setEnabled(true);
        snapShot = false;
    }


    // Update the waveforms
    //    updateWaveforms();
}

void MainWindow::highlightRisingEdge() {
    if (oscSettings.triggerType == RisingEdgeHighlighter) {
        oscSettings.triggerType = NoTrigger; // Toggle off if already set
        logInfo("Rising edge deselected");
    } else {
        oscSettings.triggerType = RisingEdgeHighlighter;
        logInfo("Rising edge selected");
    }
    //    updateWaveforms();
}

void MainWindow::highlightFallingEdge() {
    if (oscSettings.triggerType == FallingEdgeHighlighter) {
        oscSettings.triggerType = NoTrigger; // Toggle off if already set
        logInfo("Falling edge deselected");
    } else {
        oscSettings.triggerType = FallingEdgeHighlighter;
        logInfo("Falling edge selected");
    }
    //    updateWaveforms();
}

void MainWindow::setTriggerLevel() {
    bool ok;
    double level = ui->triggerValue->toPlainText().toDouble(&ok);

    if(oscSettings.triggerType == TriggerLevel){
        oscSettings.triggerType = NoTrigger; // Toggle off if already set
        logInfo("Trigger deselected");
        isTrig1Hit = false;
        isTrig2Hit = false;
        updateWaveforms();

    } else {

        if (!ok) {
            ui->triggerValue->clear();
            logInfo("ERROR: Invalid input. Please enter a valid number.");
            return;
        }

        oscSettings.triggerType = TriggerLevel;
        oscSettings.triggerLevel = level;
        logInfo("Trigger level set to: " + QString::number(level));

        //        updateWaveforms();
    }
}



void MainWindow::analyzeWaveformData() {
    bool triggerLevelReachedChannel1 = std::any_of(waveformData.channel1.begin(), waveformData.channel1.end(), [this](double value) {
        return oscSettings.triggerType == TriggerLevel && std::abs(value) >= oscSettings.triggerLevel;
    });

    bool triggerLevelReachedChannel2 = std::any_of(waveformData.channel2.begin(), waveformData.channel2.end(), [this](double value) {
        return oscSettings.triggerType == TriggerLevel && std::abs(value) >= oscSettings.triggerLevel;
    });

    if (triggerLevelReachedChannel1 && !isTrig1Hit) {
        logInfo("Trigger Level reached: Wave 1, trigger level: " + QString::number(oscSettings.triggerLevel));
        isTrig1Hit = true;
        snapShotData.channel1 = waveformData.channel1;
    }

    if (triggerLevelReachedChannel2 && !isTrig2Hit) {
        logInfo("Trigger Level reached: Wave 2, trigger level: " + QString::number(oscSettings.triggerLevel));
        isTrig2Hit = true;
        snapShotData.channel2 = waveformData.channel2;
    }

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

    if (isSampling) {
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

    logInfo("attempted to turn on");
}

void MainWindow::turnOffBoard() {
    // GO BIT
    QString dataStr = "0";
    QString addressStr = "ffffffb0";
    bool isHex = true;
    bool debug = false;
//    onPoke(addressStr, dataStr, isHex, debug);


    addressStr = "fffffff4";
    onPoke(addressStr, dataStr, isHex, debug);

    addressStr = "fffffff0";
    onPoke(addressStr, dataStr, isHex, debug);

    logInfo("attempted to turn off");
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

        turnOffBoard();

        isSampling = false;
        ui->startSampling->setText("Start Sampling");
        logInfo("..... STOPPING .....");
        disconnect(&serial, &QSerialPort::readyRead, this, &MainWindow::Sampling);

        ui->connectButton->setText("Connect");
        ui->connectButton->setStyleSheet("color: red; background-color: white;");
        logInfo("Disconnected from "+ serial.portName());


        serial.close();

    } else {
        serial.setPortName(ui->comPortComboBox->currentText()); // Ensure this is correct
        serial.setBaudRate(921600);

        if (serial.open(QIODevice::ReadWrite)) {
            ui->connectButton->setText("Disconnect");
            ui->connectButton->setStyleSheet("color: green; background-color: white;");
            logInfo("Connected to " + serial.portName());

            turnOnBoard();

            // read SPI
            // onDataSliderInit();

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
    zoomLevel = 30.0; // Reset the zoom level to default
    updateWaveforms();
}

void MainWindow::onZoomIn() {
    zoomLevel *= 0.9; // Increase the zoom level by 10%
    updateWaveforms();
}


// ---------------------------------------------


MainWindow::~MainWindow()
{

    if (serial.isOpen()) {
        disconnect(&serial, &QSerialPort::readyRead, this, &MainWindow::Sampling);

        turnOffBoard();

        serial.close();
    }

    delete ui;
}
