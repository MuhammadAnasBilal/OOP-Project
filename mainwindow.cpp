#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QDebug>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWindow),
    trafficSystem(new TrafficSystem(this)),
    uiUpdateTimer(new QTimer(this))
{
    ui->setupUi(this);
    this->setWindowTitle("ㅤㅤSmart Traffic Management System");

    if (!trafficSystem->initializeSystem()) {
        QMessageBox::critical(this, "System Initialization Failed",
                              "The traffic system backend failed to initialize. "
                              "This is likely due to missing model files (yolov8n.onnx, coco.names). "
                              "Please ensure they are in the application directory.");
        QTimer::singleShot(0, this, &QWidget::close); // Close app if backend fails
        return;
    }

    initializeUiConnections();
    connectTrafficSystemSignals();

    ui->violations_tableWidget->setColumnCount(3);
    ui->violations_tableWidget->setHorizontalHeaderLabels({"Timestamp", "Road", "Reason"});
    ui->violations_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->violations_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->logs_logDisplay->setFont(QFont("Monospace", 9));

    populateArduinoPortsCombobox();
    updateStatusbar();

    connect(uiUpdateTimer, &QTimer::timeout, this, &MainWindow::onUiUpdateTimerTimeout);
    uiUpdateTimer->start(1000); // Update timers once per second
    addLogMessage("UI and TrafficSystem initialized. System ready.", "INFO");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initializeUiConnections()
{
    connect(ui->sysctrl_startSystemBtn, &QPushButton::clicked, this, &MainWindow::onStartSystemClicked);
    connect(ui->sysctrl_stopSystemBtn, &QPushButton::clicked, this, &MainWindow::onStopSystemClicked);

    connect(ui->sysctrl_connectCamera1Btn, &QPushButton::clicked, this, [this](){ onConnectCameraClicked(0); });
    connect(ui->sysctrl_connectCamera2Btn, &QPushButton::clicked, this, [this](){ onConnectCameraClicked(1); });
    connect(ui->sysctrl_connectCamera3Btn, &QPushButton::clicked, this, [this](){ onConnectCameraClicked(2); });
    connect(ui->sysctrl_connectCamera4Btn, &QPushButton::clicked, this, [this](){ onConnectCameraClicked(3); });
    connect(ui->sysctrl_disconnectCamera1Btn, &QPushButton::clicked, this, [this](){ onDisconnectCameraClicked(0); });
    connect(ui->sysctrl_disconnectCamera2Btn, &QPushButton::clicked, this, [this](){ onDisconnectCameraClicked(1); });
    connect(ui->sysctrl_disconnectCamera3Btn, &QPushButton::clicked, this, [this](){ onDisconnectCameraClicked(2); });
    connect(ui->sysctrl_disconnectCamera4Btn, &QPushButton::clicked, this, [this](){ onDisconnectCameraClicked(3); });

    connect(ui->sysctrl_refreshPortsBtn, &QPushButton::clicked, this, &MainWindow::onRefreshArduinoPortsClicked);
    connect(ui->sysctrl_arduinoPortCombo, &QComboBox::currentTextChanged, this, &MainWindow::onArduinoPortSelected);
    connect(ui->sysctrl_simulationModeCheck, &QCheckBox::toggled, this, &MainWindow::onArduinoSimulationToggled);

    connect(ui->settings_applyTrafficBtn, &QPushButton::clicked, this, &MainWindow::onApplyTrafficSettingsClicked);

    connect(ui->violations_openFolderBtn, &QPushButton::clicked, this, &MainWindow::onOpenViolationsFolderClicked);
    connect(ui->violations_clearTableBtn, &QPushButton::clicked, this, &MainWindow::onClearViolationsTableClicked);

    connect(ui->logs_exportLogBtn, &QPushButton::clicked, this, &MainWindow::onExportLogsClicked);
    connect(ui->logs_clearLogBtn, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
}

void MainWindow::connectTrafficSystemSignals()
{
    connect(trafficSystem, &TrafficSystem::vehicleCountChanged, this, &MainWindow::handleVehicleCountChanged);
    connect(trafficSystem, &TrafficSystem::densityChanged, this, &MainWindow::handleDensityChanged);
    connect(trafficSystem, &TrafficSystem::trafficLightChanged, this, &MainWindow::handleTrafficLightChanged);
    connect(trafficSystem, &TrafficSystem::violationDetected, this, &MainWindow::handleViolationDetected, Qt::QueuedConnection);
    connect(trafficSystem, &TrafficSystem::frameUpdated, this, &MainWindow::handleFrameUpdated, Qt::QueuedConnection);
    connect(trafficSystem, &TrafficSystem::cameraStatusChanged, this, &MainWindow::handleCameraStatusChanged);
    connect(trafficSystem, &TrafficSystem::arduinoStatusChanged, this, &MainWindow::handleArduinoStatusChanged);
    connect(trafficSystem, &TrafficSystem::energySavingStatusChanged, this, &MainWindow::handleEnergySavingStatusChanged);
    connect(trafficSystem, &TrafficSystem::logMessage, this, &MainWindow::addLogMessage, Qt::QueuedConnection);
}

void MainWindow::onStartSystemClicked() {
    trafficSystem->startSystem();
    ui->sysctrl_startSystemBtn->setEnabled(false);
    ui->sysctrl_stopSystemBtn->setEnabled(true);
}

void MainWindow::onStopSystemClicked() {
    trafficSystem->stopSystem();
    ui->sysctrl_startSystemBtn->setEnabled(true);
    ui->sysctrl_stopSystemBtn->setEnabled(false);
}

void MainWindow::onConnectCameraClicked(int roadIndex) {
    QLineEdit* edits[] = {ui->sysctrl_camera1Edit, ui->sysctrl_camera2Edit, ui->sysctrl_camera3Edit, ui->sysctrl_camera4Edit};
    if (roadIndex < 0 || roadIndex >= 4) return;

    QString source = edits[roadIndex]->text();
    if (source.isEmpty()) {
        QMessageBox::warning(this, "Input Error", QString("Camera source for Road %1 cannot be empty.").arg(roadIndex + 1));
        return;
    }
    trafficSystem->connectCamera(roadIndex, source);
}

void MainWindow::onDisconnectCameraClicked(int roadIndex) {
    trafficSystem->disconnectCamera(roadIndex);
}

void MainWindow::onRefreshArduinoPortsClicked() {
    populateArduinoPortsCombobox();
}

void MainWindow::onArduinoPortSelected(const QString& portName){
    if(portName.contains("No ports") || portName.isEmpty()) return;
    trafficSystem->initializeArduino(portName);
}

void MainWindow::onArduinoSimulationToggled(bool checked){
    trafficSystem->setArduinoSimulationMode(checked);
    ui->sysctrl_arduinoPortCombo->setDisabled(checked);
    ui->sysctrl_refreshPortsBtn->setDisabled(checked);
}

void MainWindow::onApplyTrafficSettingsClicked() {
    trafficSystem->setLightTiming(TrafficDensity::LOW, ui->settings_lowDensityTimeSpin->value());
    trafficSystem->setLightTiming(TrafficDensity::MEDIUM, ui->settings_mediumDensityTimeSpin->value());
    trafficSystem->setLightTiming(TrafficDensity::HIGH, ui->settings_highDensityTimeSpin->value());
    trafficSystem->setLightTiming(TrafficDensity::VERY_HIGH, ui->settings_veryHighDensityTimeSpin->value());
    trafficSystem->setYellowLightDuration(ui->settings_yellowLightTimeSpin->value());
    trafficSystem->setEnergySavingEnabled(ui->settings_energySavingCheck->isChecked());
    trafficSystem->setViolationDetectionEnabled(ui->settings_violationDetectionCheck->isChecked());
    trafficSystem->setYoloThresholds(
        static_cast<float>(ui->settings_yoloConfidenceSpin->value()),
        static_cast<float>(ui->settings_yoloNMSSpin->value())
        );
    addLogMessage("Settings applied to the backend.", "ACTION");
}

void MainWindow::onOpenViolationsFolderClicked() {
    QString dir = trafficSystem->getViolationDirectory();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir))) {
        QMessageBox::warning(this, "Error", "Could not open violations folder at: " + dir);
    }
}

void MainWindow::onClearViolationsTableClicked() {
    ui->violations_tableWidget->setRowCount(0);
}

void MainWindow::onExportLogsClicked() {
    QString fileName = QFileDialog::getSaveFileName(this, "Export Logs", QDir::homePath() + "/stms_log.txt", "Text Files (*.txt)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << ui->logs_logDisplay->toPlainText();
        file.close();
        addLogMessage("Log exported to: " + fileName, "INFO");
    } else {
        QMessageBox::critical(this, "Export Error", "Could not write to file: " + file.errorString());
    }
}

void MainWindow::onClearLogClicked(){
    ui->logs_logDisplay->clear();
}


void MainWindow::handleVehicleCountChanged(int roadIndex, int count) {
    QLabel* labels[] = {ui->monitor_vehicleCount1, ui->monitor_vehicleCount2, ui->monitor_vehicleCount3, ui->monitor_vehicleCount4};
    if (roadIndex >= 0 && roadIndex < 4) {
        labels[roadIndex]->setText(QString("Vehicles: %1").arg(count));
    }
}

void MainWindow::handleDensityChanged(int roadIndex, TrafficDensity density) {
    QLabel* labels[] = {ui->monitor_density1, ui->monitor_density2, ui->monitor_density3, ui->monitor_density4};
    if (roadIndex >= 0 && roadIndex < 4) {
        labels[roadIndex]->setText(QString("Density: %1").arg(formatDensityToString(density)));
    }
}

void MainWindow::handleTrafficLightChanged(int roadIndex, TrafficLight light) {
    if (roadIndex < 0 || roadIndex >= 4) return;

    // Update monitor text
    QLabel* monitorLabels[] = {ui->monitor_lightStatus1, ui->monitor_lightStatus2, ui->monitor_lightStatus3, ui->monitor_lightStatus4};
    QString lightText = (light == TrafficLight::RED) ? "RED" : (light == TrafficLight::YELLOW) ? "YELLOW" : (light == TrafficLight::GREEN) ? "GREEN" : "OFF";
    monitorLabels[roadIndex]->setText(QString("Light: %1").arg(lightText));

    // Update visual indicator
    QLabel* indicators[] = {ui->lights_light1Red, ui->lights_light1Yellow, ui->lights_light1Green,
                            ui->lights_light2Red, ui->lights_light2Yellow, ui->lights_light2Green,
                            ui->lights_light3Red, ui->lights_light3Yellow, ui->lights_light3Green,
                            ui->lights_light4Red, ui->lights_light4Yellow, ui->lights_light4Green};

    styleLightIndicatorLabel(indicators[roadIndex * 3 + 0], light == TrafficLight::RED ? light : TrafficLight::OFF);
    styleLightIndicatorLabel(indicators[roadIndex * 3 + 1], light == TrafficLight::YELLOW ? light : TrafficLight::OFF);
    styleLightIndicatorLabel(indicators[roadIndex * 3 + 2], light == TrafficLight::GREEN ? light : TrafficLight::OFF);

    if (light == TrafficLight::GREEN) {
        ui->lights_currentRoadLabel->setText(QString("Current Green: Road %1").arg(roadIndex + 1));
    }
}

void MainWindow::handleViolationDetected(int roadIndex, const QString& timestamp, const QString& reason, const cv::Mat& frame) {
    addViolationEntryToTable(roadIndex, timestamp, reason);
    if (!frame.empty()) {
        QString filename = QString("VIO_%1_R%2.jpg").arg(timestamp).arg(roadIndex + 1);
        QString fullPath = QDir(trafficSystem->getViolationDirectory()).filePath(filename);
        if (!cv::imwrite(fullPath.toStdString(), frame)) {
            addLogMessage("Failed to save violation image: " + fullPath, "ERROR");
        }
    }
}


void MainWindow::handleFrameUpdated(int roadIndex, const QImage& frame) {
    QLabel* displays[] = {ui->monitor_cameraDisplay1, ui->monitor_cameraDisplay2, ui->monitor_cameraDisplay3, ui->monitor_cameraDisplay4};
    if (roadIndex >= 0 && roadIndex < 4 && !frame.isNull()) {
        displays[roadIndex]->setPixmap(QPixmap::fromImage(frame));
    }
}

void MainWindow::handleCameraStatusChanged(int roadIndex, bool connected) {
    QLabel* displays[] = {ui->monitor_cameraDisplay1, ui->monitor_cameraDisplay2, ui->monitor_cameraDisplay3, ui->monitor_cameraDisplay4};
    if (roadIndex >= 0 && roadIndex < 4 && !connected) {
        displays[roadIndex]->clear();
        displays[roadIndex]->setText("Feed Off / Disconnected");
        displays[roadIndex]->setStyleSheet("background-color: #333; color: #888;");
    }
    updateStatusbar();
}

void MainWindow::handleArduinoStatusChanged(bool connected, const QString& portName) {
    if (connected) {
        int index = ui->sysctrl_arduinoPortCombo->findText(portName);
        if (index != -1) {
            ui->sysctrl_arduinoPortCombo->setCurrentIndex(index);
        }
    } else if (portName == "Simulation"){

    } else {

    }
    updateStatusbar();
}

void MainWindow::handleEnergySavingStatusChanged(bool active) {
    if (active) ui->lights_currentRoadLabel->setText("Energy Saving (Lights OFF)");
    updateStatusbar();
}

void MainWindow::addLogMessage(const QString& message, const QString& level) {
    QString color = "white";
    if (level == "ERROR") color = "#FF5555";
    else if (level == "WARNING") color = "#FFAA00";
    else if (level == "INFO") color = "#55FFFF";
    else if (level == "ACTION") color = "lightgreen";
    QString formattedMessage = QString("<font color='%1'>[%2] [%3] %4</font>")
                                   .arg(color)
                                   .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                                   .arg(level.toUpper().leftJustified(7))
                                   .arg(message.toHtmlEscaped());
    ui->logs_logDisplay->append(formattedMessage);
}


void MainWindow::onUiUpdateTimerTimeout() {
    updateStatusbar();
    if (trafficSystem && trafficSystem->isSystemRunning()) {
        int remaining = trafficSystem->getCurrentLightTimeRemaining();
        int currentRoad = trafficSystem->getCurrentRoadIndex();
        int totalDuration = 0;
        if (trafficSystem->getCurrentLight(currentRoad) == TrafficLight::GREEN) {
            totalDuration = trafficSystem->getRedLightDuration(trafficSystem->getRoadData(currentRoad).density);
        } else if (trafficSystem->getCurrentLight(currentRoad) == TrafficLight::YELLOW) {
            totalDuration = trafficSystem->getYellowLightDuration();
        }

        if (totalDuration > 0) {
            ui->lights_lightTimerProgress->setRange(0, totalDuration);
            ui->lights_lightTimerProgress->setValue(totalDuration - remaining);
            ui->lights_lightTimerProgress->setFormat(QString("%1s / %2s").arg(totalDuration - remaining).arg(totalDuration));
        } else {
            ui->lights_lightTimerProgress->setValue(0);
            ui->lights_lightTimerProgress->setFormat("N/A");
        }
    }
}

void MainWindow::updateStatusbar() {
    QStringList status;
    status << (trafficSystem->isSystemRunning() ? "Running" : "Stopped");
    if (trafficSystem->isEnergySavingActive()) status << "EnergySaving";

    int connectedCams = 0;
    for(int i = 0; i < 4; ++i) if(trafficSystem->getRoadData(i).cameraConnected) connectedCams++;
    status << QString("Cams:%1/4").arg(connectedCams);

    if (ui->sysctrl_simulationModeCheck->isChecked()){
        status << "Arduino:Sim";
    } else {
        status << "Arduino:" + (trafficSystem->getArduinoData().connected ? trafficSystem->getArduinoData().portName : "Off");
    }
    ui->statusbar->showMessage(status.join(" | "));
}

void MainWindow::populateArduinoPortsCombobox() {
    ui->sysctrl_arduinoPortCombo->clear();
    QStringList portList = trafficSystem->getAvailableArduinoPorts();
    if (portList.isEmpty()) {
        ui->sysctrl_arduinoPortCombo->addItem("No ports found");
        ui->sysctrl_arduinoPortCombo->setEnabled(false);
    } else {
        ui->sysctrl_arduinoPortCombo->addItems(portList);
        ui->sysctrl_arduinoPortCombo->setEnabled(true);
    }
}

void MainWindow::styleLightIndicatorLabel(QLabel* label, TrafficLight light) {
    QString colorStyle = "#333"; // Off
    if (light == TrafficLight::RED) colorStyle = "red";
    else if (light == TrafficLight::YELLOW) colorStyle = "gold";
    else if (light == TrafficLight::GREEN) colorStyle = "lime";
    label->setStyleSheet(QString("background-color: %1; border-radius: %2px; border: 1px solid #555;")
                             .arg(colorStyle).arg(label->height() / 2));
}

void MainWindow::addViolationEntryToTable(int roadIndex, const QString& timestamp, const QString& reason) {
    int row = ui->violations_tableWidget->rowCount();
    ui->violations_tableWidget->insertRow(row);
    ui->violations_tableWidget->setItem(row, 0, new QTableWidgetItem(QDateTime::fromString(timestamp, "yyyy-MM-dd_hh-mm-ss-zzz").toString("yyyy-MM-dd hh:mm:ss")));
    ui->violations_tableWidget->setItem(row, 1, new QTableWidgetItem(QString("Road %1").arg(roadIndex + 1)));
    ui->violations_tableWidget->setItem(row, 2, new QTableWidgetItem(reason));
}

QString MainWindow::formatDensityToString(TrafficDensity density) const {
    switch(density) {
    case TrafficDensity::OFF: return "OFF";
    case TrafficDensity::LOW: return "LOW";
    case TrafficDensity::MEDIUM: return "MEDIUM";
    case TrafficDensity::HIGH: return "HIGH";
    case TrafficDensity::VERY_HIGH: return "V.HIGH";
    }
    return "N/A";
}


void MainWindow::closeEvent(QCloseEvent *event) {
    if (trafficSystem && trafficSystem->isSystemRunning()) {
        auto reply = QMessageBox::question(this, "Exit Confirmation",
                                           "The traffic system is running. Are you sure you want to exit?",
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}
