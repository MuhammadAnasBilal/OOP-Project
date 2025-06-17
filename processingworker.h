#ifndef PROCESSINGWORKER_H
#define PROCESSINGWORKER_H

#include <QObject>
#include <QImage>
#include <QDateTime>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include "traffic_types.h"
#include <array>
#include <map>
#include <vector>

struct TrackedVehicle {
    int id;
    cv::Rect boundingBox;
    int framesWithoutDetection = 0;
    bool isViolationCandidate = false;
    int violationFrameCount = 0;
};

class ProcessingWorker : public QObject
{
    Q_OBJECT

public:
    explicit ProcessingWorker(QObject *parent = nullptr);
    ~ProcessingWorker();

    bool initializeModels(const QString& yoloModelPath, const QString& cocoNamesPath);

public slots:
    void processFrame(int roadIndex, cv::Mat frame, cv::Rect roi, TrafficLight currentLight);
    void setYoloThresholds(float confidence, float nms);

signals:
    void processingFinished(int roadIndex, const QImage& displayFrame, int vehicleCount, const std::vector<int>& violatingVehicleIDs);
    void logMessage(const QString& message, const QString& level);

private:

    cv::dnn::Net yoloNet;
    std::vector<std::string> classNames;
    bool yoloInitialized = false;
    float yoloConfidenceThreshold = 0.45f;
    float yoloNmsThreshold = 0.4f;


    std::array<std::map<int, TrackedVehicle>, 4> roadTrackers;
    std::array<int, 4> nextVehicleID;

    void detectAndTrack(int roadIndex, cv::Mat &frame, TrafficLight currentLight);
    std::vector<cv::Rect> detectVehiclesYOLO(const cv::Mat& frame);
    void updateTrackers(int roadIndex, const std::vector<cv::Rect>& detections, TrafficLight currentLight);

    QImage matToQImage(const cv::Mat& mat);
    void drawDetections(cv::Mat& frame, int roadIndex);
};

#endif // PROCESSINGWORKER_H
