#include "trafficsystem.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QSerialPortInfo>
#include <QThread>

// Register custom types for signal-slot mechanism
Q_DECLARE_METATYPE(cv::Mat);
Q_DECLARE_METATYPE(cv::Rect);
Q_DECLARE_METATYPE(TrafficLight);

TrafficSystem::TrafficSystem(QObject *parent)
    : QObject(parent),
    systemRunning(false),
    processingThread(nullptr),
    worker(nullptr),
    m_workerBusy(false),
    currentRoadIndex(0),
    lightTimeRemaining(0),
    yellowLightActive(false),
    yellowLightFixedDuration(3), // 3s default yellow
    energySavingMode(false),
    energySavingEnabled(true),
    violationDetectionEnabled(true),
    arduino(nullptr)
{
    qRegisterMetaType<cv::Mat>();
    qRegisterMetaType<cv::Rect>();
    qRegisterMetaType<TrafficLight>();

    // Default light durations
    currentLights.fill(TrafficLight::OFF);
    lightDurations[static_cast<int>(TrafficDensity::OFF)] = 5;
    lightDurations[static_cast<int>(TrafficDensity::LOW)] = 8;
    lightDurations[static_cast<int>(TrafficDensity::MEDIUM)] = 12;
    lightDurations[static_cast<int>(TrafficDensity::HIGH)] = 18;
    lightDurations[static_cast<int>(TrafficDensity::VERY_HIGH)] = 25;

    // Setup violation directory
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dataPath.isEmpty()) dataPath = QDir::currentPath();
    violationDir = QDir(dataPath).absoluteFilePath("stms_violations");
    QDir().mkpath(violationDir);
}

TrafficSystem::~TrafficSystem() {
    stopSystem();
    if (processingThread) {
        processingThread->quit();
        processingThread->wait();
    }
    delete arduino;
}

bool TrafficSystem::initializeSystem() {
    emit logMessage("Initializing Traffic System...", "INFO");
    processingThread = new QThread(this);
    worker = new ProcessingWorker();

    if(!worker->initializeModels("yolov8n.onnx", "coco.names")){
        emit logMessage("Failed to initialize ML models. System cannot start.", "ERROR");
        delete worker; worker = nullptr;
        delete processingThread; processingThread = nullptr;
        return false;
    }

    worker->moveToThread(processingThread);

    connect(processingThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &TrafficSystem::requestFrameProcessing, worker, &ProcessingWorker::processFrame, Qt::QueuedConnection);
    connect(worker, &ProcessingWorker::processingFinished, this, &TrafficSystem::handleProcessingFinished, Qt::QueuedConnection);
    connect(worker, &ProcessingWorker::logMessage, this, &TrafficSystem::handleWorkerLog, Qt::QueuedConnection);
    connect(this, &TrafficSystem::requestYoloThresholdUpdate, worker, &ProcessingWorker::setYoloThresholds, Qt::QueuedConnection);

    processingThread->start();
    emit logMessage("Processing worker thread started.", "INFO");

    initializeTimers();
    initializeArduino();
    return true;
}

void TrafficSystem::initializeTimers() {
    mainTimer = new QTimer(this);
    lightTimer = new QTimer(this);
    sensorTimer = new QTimer(this);
    connect(mainTimer, &QTimer::timeout, this, &TrafficSystem::onMainTimerTimeout);
    connect(lightTimer, &QTimer::timeout, this, &TrafficSystem::onLightTimerTimeout);
    connect(sensorTimer, &QTimer::timeout, this, &TrafficSystem::onSensorTimerTimeout);
}

void TrafficSystem::startSystem() {
    if (systemRunning) return;
    systemRunning = true;
    currentRoadIndex = 0;
    yellowLightActive = false;
    lightTimeRemaining = 0;
    mainTimer->start(50);
    if(arduinoData.connected) sensorTimer->start(250);
    processTrafficCycle();
    emit logMessage("Traffic system started.", "INFO");
}

void TrafficSystem::stopSystem() {
    if (!systemRunning) return;
    systemRunning = false;
    mainTimer->stop();
    lightTimer->stop();
    sensorTimer->stop();
    setAllTrafficLights(energySavingEnabled ? TrafficLight::OFF : TrafficLight::RED);
    emit logMessage("Traffic system stopped.", "INFO");
}

void TrafficSystem::onSensorTimerTimeout() {
    if (arduinoData.connected) {
        sendArduinoCommand("GET_SENSORS");
    }
}

void TrafficSystem::onMainTimerTimeout() {
    if (!systemRunning || m_workerBusy) return;

    static int roadToProcess = 0;
    roadToProcess = (roadToProcess + 1) % 4;

    if (roads[roadToProcess].cameraConnected && roads[roadToProcess].camera.isOpened()) {
        cv::Mat frame;
        if (roads[roadToProcess].camera.read(frame) && !frame.empty()) {
            m_workerBusy = true;
            {
                QMutexLocker locker(&roads[roadToProcess].frameMutex);
                roads[roadToProcess].currentFrame = frame.clone();
            }
            emit requestFrameProcessing(roadToProcess, frame.clone(), roads[roadToProcess].roi, currentLights[roadToProcess]);
        }
    }
}

void TrafficSystem::handleProcessingFinished(int roadIndex, const QImage& displayFrame, int vehicleCount, const std::vector<int>& violatingVehicleIDs)
{
    m_workerBusy = false;
    if (roadIndex < 0 || roadIndex >= 4) return;

    if(roads[roadIndex].vehicleCount != vehicleCount) {
        roads[roadIndex].vehicleCount = vehicleCount;
        emit vehicleCountChanged(roadIndex, vehicleCount);
        TrafficDensity newDensity = (vehicleCount < 3) ? TrafficDensity::OFF : (vehicleCount <= 4) ? TrafficDensity::LOW : (vehicleCount <= 6) ? TrafficDensity::MEDIUM : (vehicleCount <= 9) ? TrafficDensity::HIGH : TrafficDensity::VERY_HIGH;
        if(roads[roadIndex].density != newDensity){
            roads[roadIndex].density = newDensity;
            emit densityChanged(roadIndex, newDensity);
        }
    }

    emit frameUpdated(roadIndex, displayFrame);

    if (violationDetectionEnabled) {
        for(int id : violatingVehicleIDs) {
            if(roads[roadIndex].violatedIDs.find(id) == roads[roadIndex].violatedIDs.end()){
                QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
                cv::Mat frameCopy;
                {
                    QMutexLocker locker(&roads[roadIndex].frameMutex);
                    if(!roads[roadIndex].currentFrame.empty()) frameCopy = roads[roadIndex].currentFrame.clone();
                }
                emit violationDetected(roadIndex, timestamp, QString("Vehicle ID %1 ran red light").arg(id), frameCopy);
                roads[roadIndex].violatedIDs.insert(id);
            }
        }
    }
    updateTrafficLights();
}

void TrafficSystem::onLightTimerTimeout() {
    if (!systemRunning || energySavingMode) {
        lightTimer->stop();
        return;
    }
    if (lightTimeRemaining > 0) {
        lightTimeRemaining--;
    }
    if (lightTimeRemaining <= 0) {
        lightTimer->stop();
        switchToNextRoad();
    }
}

void TrafficSystem::updateTrafficLights() {
    if (!systemRunning) return;
    processEnergySaving();
    if (energySavingMode) return;
    if(currentLights[currentRoadIndex] == TrafficLight::OFF && roads[currentRoadIndex].vehicleCount > 0){
        processTrafficCycle();
    }
}

void TrafficSystem::processTrafficCycle() {
    if (!systemRunning || energySavingMode || yellowLightActive || lightTimer->isActive()) return;

    for (int i = 0; i < 4; ++i) {
        setTrafficLight(i, (i == currentRoadIndex) ? TrafficLight::GREEN : TrafficLight::RED);
    }
    lightTimeRemaining = getRedLightDuration(roads[currentRoadIndex].density);
    lightTimer->start(1000);
}

void TrafficSystem::switchToNextRoad() {
    if (energySavingMode) return;

    if (!yellowLightActive) {
        setTrafficLight(currentRoadIndex, TrafficLight::YELLOW);
        yellowLightActive = true;
        lightTimeRemaining = yellowLightFixedDuration;
        lightTimer->start(1000);
    } else {
        yellowLightActive = false;
        setTrafficLight(currentRoadIndex, TrafficLight::RED);
        roads[currentRoadIndex].violatedIDs.clear();
        currentRoadIndex = (currentRoadIndex + 1) % 4;
        roads[currentRoadIndex].violatedIDs.clear();
        processTrafficCycle();
    }
}

void TrafficSystem::setAllTrafficLights(TrafficLight light) {
    for (int i = 0; i < 4; ++i) {
        setTrafficLight(i, light);
    }
}

void TrafficSystem::setTrafficLight(int roadIndex, TrafficLight light) {
    if (roadIndex < 0 || roadIndex >= 4 || currentLights[roadIndex] == light) return;
    currentLights[roadIndex] = light;
    emit trafficLightChanged(roadIndex, light);
    char l_char = 'F';
    if (light == TrafficLight::RED) l_char = 'R';
    else if (light == TrafficLight::YELLOW) l_char = 'Y';
    else if (light == TrafficLight::GREEN) l_char = 'G';
    sendArduinoCommand(QString("L_%1_%2").arg(roadIndex).arg(l_char));
}

void TrafficSystem::processEnergySaving() {
    if (!energySavingEnabled) {
        if (energySavingMode) {
            energySavingMode = false;
            emit energySavingStatusChanged(false);
            processTrafficCycle();
        }
        return;
    }
    bool allRoadsEmpty = true;
    for (int i = 0; i < 4; ++i) {
        if (roads[i].cameraConnected && roads[i].vehicleCount > 0) {
            allRoadsEmpty = false;
            break;
        }
    }
    if (allRoadsEmpty && !energySavingMode) {
        energySavingMode = true;
        lightTimer->stop();
        setAllTrafficLights(TrafficLight::OFF);
        emit energySavingStatusChanged(true);
    } else if (!allRoadsEmpty && energySavingMode) {
        energySavingMode = false;
        emit energySavingStatusChanged(false);
        processTrafficCycle();
    }
}

bool TrafficSystem::connectCamera(int roadIndex, const QString& source) {
    if (roadIndex < 0 || roadIndex >= 4) return false;
    disconnectCamera(roadIndex);
    cv::VideoCapture cap;
    bool isNumeric;
    int camIndex = source.toInt(&isNumeric);
    if (isNumeric) cap.open(camIndex, cv::CAP_ANY);
    else cap.open(source.toStdString());
    if (cap.isOpened()) {
        roads[roadIndex].camera = std::move(cap);
        roads[roadIndex].camera.set(cv::CAP_PROP_BUFFERSIZE, 1);
        roads[roadIndex].cameraConnected = true;
        roads[roadIndex].cameraSource = source;
        emit cameraStatusChanged(roadIndex, true);
        emit logMessage(QString("Camera %1 connected to source: %2").arg(roadIndex + 1).arg(source), "INFO");
        return true;
    }
    emit logMessage("Failed to open camera source: " + source, "ERROR");
    return false;
}

void TrafficSystem::disconnectCamera(int roadIndex) {
    if (roadIndex < 0 || roadIndex >= 4 || !roads[roadIndex].cameraConnected) return;
    if (roads[roadIndex].camera.isOpened()) {
        roads[roadIndex].camera.release();
    }
    roads[roadIndex].vehicleCount = 0;
    roads[roadIndex].density = TrafficDensity::OFF;
    roads[roadIndex].cameraConnected = false;
    roads[roadIndex].cameraSource.clear();
    roads[roadIndex].roi = cv::Rect(0,0,0,0);
    roads[roadIndex].violatedIDs.clear();
    emit cameraStatusChanged(roadIndex, false);
    emit logMessage(QString("Camera %1 disconnected.").arg(roadIndex + 1), "INFO");
}

bool TrafficSystem::initializeArduino(const QString& portName) {
    if (arduino && arduino->isOpen()) {
        arduino->close();
    }
    if (!arduino) {
        arduino = new QSerialPort(this);
        connect(arduino, &QSerialPort::readyRead, this, &TrafficSystem::onArduinoDataReceived);
        connect(arduino, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error){
            if (error != QSerialPort::NoError && arduinoData.connected) {
                emit logMessage("Arduino Error: " + arduino->errorString(), "ERROR");
                arduinoData.connected = false;
                emit arduinoStatusChanged(false, "");
            }
        });
    }

    QString portToUse = portName;
    if (portToUse.isEmpty()) {
        const auto ports = QSerialPortInfo::availablePorts();
        if (!ports.isEmpty()) portToUse = ports.first().portName();
    }

    if (portToUse.isEmpty()) {
        emit logMessage("No Arduino ports found. Using simulation.", "WARNING");
        return false;
    }

    arduino->setPortName(portToUse);
    arduino->setBaudRate(QSerialPort::Baud9600);
    if (arduino->open(QIODevice::ReadWrite)) {
        QThread::msleep(2000);
        arduino->write("INIT\n");

        arduinoData.connected = true;
        arduinoData.portName = portToUse;
        emit arduinoStatusChanged(true, portToUse);
        emit logMessage("Arduino connected on port " + portToUse, "INFO");
        if(systemRunning) sensorTimer->start(250);
        return true;
    }
    emit logMessage("Failed to open Arduino port " + portToUse, "ERROR");
    return false;
}

void TrafficSystem::sendArduinoCommand(const QString& command) {
    if (!arduinoData.connected || !arduino || !arduino->isOpen()) return;
    QMutexLocker locker(&arduinoMutex);
    arduino->write((command + "\n").toUtf8());
}

void TrafficSystem::onArduinoDataReceived() {
    if (!arduino || !arduino->isOpen()) return;
    arduinoData.buffer.append(arduino->readAll());
    while (arduinoData.buffer.contains('\n')) {
        QByteArray line = arduinoData.buffer.left(arduinoData.buffer.indexOf('\n'));
        arduinoData.buffer.remove(0, line.length() + 1);
        parseArduinoData(line);
    }
}

void TrafficSystem::parseArduinoData(const QByteArray& data) {
    if (data.startsWith("SENSORS:")) {
        QStringList states = QString::fromUtf8(data.mid(8)).split(',', Qt::SkipEmptyParts);
        if (states.length() == 4) {
            std::array<bool, 4> newStates;
            for (int i = 0; i < 4; ++i) {
                newStates[i] = (states[i] == "1");
            }

            for (int i = 0; i < 4; ++i) {
                if (newStates[i] && !arduinoData.irSensorPreviousStates[i] && !irViolationCooldownActive[i]) {
                    if (currentLights[i] == TrafficLight::RED && violationDetectionEnabled) {
                        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
                        QString reason = QString("IR sensor triggered on red light for Road %1").arg(i + 1);

                        saveViolationScreenshot(i, 1, timestamp);
                        emit violationDetected(i, timestamp, reason, roads[i].currentFrame);
                        logMessage(reason, "VIOLATION");

                        QTimer::singleShot(500, this, [this, i, timestamp]() { saveViolationScreenshot(i, 2, timestamp); });
                        QTimer::singleShot(1000, this, [this, i, timestamp]() { saveViolationScreenshot(i, 3, timestamp); });

                        irViolationCooldownActive[i] = true;
                        QTimer::singleShot(5000, this, [this, i](){ irViolationCooldownActive[i] = false; });
                    }
                }
            }
            arduinoData.irSensorPreviousStates = newStates;
        }
    }
}

void TrafficSystem::saveViolationScreenshot(int roadIndex, int imageNum, const QString& baseTimestamp) {
    cv::Mat frameCopy;
    {
        QMutexLocker locker(&roads[roadIndex].frameMutex);
        if(!roads[roadIndex].currentFrame.empty()) frameCopy = roads[roadIndex].currentFrame.clone();
    }
    if (!frameCopy.empty()) {
        QString filename = QString("VIO_IR_%1_R%2_IMG%3.jpg").arg(baseTimestamp).arg(roadIndex + 1).arg(imageNum);
        QString fullPath = QDir(violationDir).filePath(filename);
        if (!cv::imwrite(fullPath.toStdString(), frameCopy)) {
            logMessage("Failed to save IR violation image: " + fullPath, "ERROR");
        } else {
            logMessage("Saved IR violation image: " + filename, "INFO");
        }
    }
}

const RoadData& TrafficSystem::getRoadData(int idx) const { static RoadData empty; return (idx >= 0 && idx < 4) ? roads[idx] : empty; }
const ArduinoData& TrafficSystem::getArduinoData() const { return arduinoData; }
TrafficLight TrafficSystem::getCurrentLight(int idx) const { return (idx >= 0 && idx < 4) ? currentLights[idx] : TrafficLight::OFF; }
void TrafficSystem::setLightTiming(TrafficDensity d, int secs) { lightDurations[static_cast<int>(d)] = secs; }
void TrafficSystem::setYellowLightDuration(int secs) { yellowLightFixedDuration = secs; }
void TrafficSystem::setEnergySavingEnabled(bool enabled) { energySavingEnabled = enabled; }
void TrafficSystem::setViolationDetectionEnabled(bool enabled) { violationDetectionEnabled = enabled; }
void TrafficSystem::setRoadROI(int roadIndex, const cv::Rect& roi) { if (roadIndex >= 0 && roadIndex < 4) roads[roadIndex].roi = roi; }
void TrafficSystem::setYoloThresholds(float confidence, float nms) { emit requestYoloThresholdUpdate(confidence, nms); }
void TrafficSystem::handleWorkerLog(const QString& message, const QString& level) { emit logMessage(message, level); }

void TrafficSystem::setArduinoSimulationMode(bool simActive) {
    if(simActive && arduinoData.connected) {
        if(arduino && arduino->isOpen()) arduino->close();
        arduinoData.connected = false;
        sensorTimer->stop();
        emit arduinoStatusChanged(false, "Simulation");
    } else if (!simActive && !arduinoData.connected) {
        initializeArduino();
    }
}

QStringList TrafficSystem::getAvailableArduinoPorts() {
    QStringList ports;
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ports.append(info.portName());
    }
    return ports;
}

int TrafficSystem::getRedLightDuration(TrafficDensity density) {
    return lightDurations[static_cast<int>(density)];
}

QString TrafficSystem::getViolationDirectory() const {
    return violationDir;
}
