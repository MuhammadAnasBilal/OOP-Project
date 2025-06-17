#ifndef TRAFFICSYSTEM_H
#define TRAFFICSYSTEM_H

#include <QObject>
#include <QTimer>
#include <QSerialPort>
#include <QThread>
#include <QMutex>
#include <QImage>
#include <opencv2/opencv.hpp>
#include "traffic_types.h"
#include "processingworker.h"

#include <array>
#include <map>
#include <atomic>
#include <set>


struct RoadData {
    int vehicleCount = 0;
    TrafficDensity density = TrafficDensity::OFF;
    cv::VideoCapture camera;
    cv::Mat currentFrame;
    QMutex frameMutex;
    bool cameraConnected = false;
    QString cameraSource;
    cv::Rect roi = cv::Rect(0, 0, 0, 0);
    std::set<int> violatedIDs;
};

struct ArduinoData {
    bool connected = false;
    QString portName;
    QByteArray buffer;
    std::array<bool, 4> irSensorStates{false};
    std::array<bool, 4> irSensorPreviousStates{false};
};

class TrafficSystem : public QObject
{
    Q_OBJECT

public:
    explicit TrafficSystem(QObject *parent = nullptr);
    ~TrafficSystem();

    bool initializeSystem();
    void startSystem();
    void stopSystem();
    bool isSystemRunning() const { return systemRunning; }

    void setLightTiming(TrafficDensity density, int durationSeconds);
    void setYellowLightDuration(int seconds);
    void setEnergySavingEnabled(bool enabled);
    void setViolationDetectionEnabled(bool enabled);
    void setRoadROI(int roadIndex, const cv::Rect& roi);
    void setYoloThresholds(float confidence, float nms);

    bool connectCamera(int roadIndex, const QString& source);
    void disconnectCamera(int roadIndex);
    bool initializeArduino(const QString& portName = "");
    void setArduinoSimulationMode(bool simActive);
    QStringList getAvailableArduinoPorts();

    const RoadData& getRoadData(int roadIndex) const;
    const ArduinoData& getArduinoData() const;
    TrafficLight getCurrentLight(int roadIndex) const;
    int getCurrentLightTimeRemaining() const { return lightTimeRemaining; }
    int getCurrentRoadIndex() const { return currentRoadIndex; }
    int getYellowLightDuration() const { return yellowLightFixedDuration; }
    bool isEnergySavingActive() const { return energySavingMode; }
    int getRedLightDuration(TrafficDensity density);
    QString getViolationDirectory() const;


signals:

    void vehicleCountChanged(int roadIndex, int count);
    void densityChanged(int roadIndex, TrafficDensity density);
    void trafficLightChanged(int roadIndex, TrafficLight light);
    void frameUpdated(int roadIndex, const QImage& frame);
    void violationDetected(int roadIndex, const QString& timestamp, const QString& reason, const cv::Mat& frame);
    void logMessage(const QString& message, const QString& level);
    void cameraStatusChanged(int roadIndex, bool connected);
    void arduinoStatusChanged(bool connected, const QString& portName);
    void energySavingStatusChanged(bool active);


    void requestFrameProcessing(int roadIndex, cv::Mat frame, cv::Rect roi, TrafficLight currentLight);
    void requestYoloThresholdUpdate(float confidence, float nms);

private slots:
    void onMainTimerTimeout();
    void onLightTimerTimeout();
    void onArduinoDataReceived();
    void onSensorTimerTimeout();
    void handleProcessingFinished(int roadIndex, const QImage& displayFrame, int vehicleCount, const std::vector<int>& violatingVehicleIDs);
    void handleWorkerLog(const QString& message, const QString& level);

private:
    std::array<RoadData, 4> roads;
    std::array<TrafficLight, 4> currentLights;
    std::atomic<bool> systemRunning;

    QThread* processingThread;
    ProcessingWorker* worker;
    std::atomic<bool> m_workerBusy;

    int currentRoadIndex;
    int lightTimeRemaining;
    bool yellowLightActive;
    int yellowLightFixedDuration;
    bool energySavingMode;
    bool energySavingEnabled;
    std::array<int, 5> lightDurations;
    bool violationDetectionEnabled;
    QString violationDir;
    std::array<bool, 4> irViolationCooldownActive{false};

    QTimer *mainTimer;
    QTimer *lightTimer;
    QTimer *sensorTimer;
    QSerialPort *arduino;
    ArduinoData arduinoData;
    QMutex arduinoMutex;

    // Helper Methods
    void initializeTimers();
    void updateTrafficLights();
    void processTrafficCycle();
    void switchToNextRoad();
    void setAllTrafficLights(TrafficLight light);
    void setTrafficLight(int roadIndex, TrafficLight light);
    void processEnergySaving();
    void sendArduinoCommand(const QString& command);
    void parseArduinoData(const QByteArray& data);
    void saveViolationScreenshot(int roadIndex, int imageNum, const QString& baseTimestamp);
};

#endif // TRAFFICSYSTEM_H
