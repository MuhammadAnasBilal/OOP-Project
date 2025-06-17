#include "processingworker.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>
#include <algorithm>

// COCO class IDs for vehicles: car, motorcycle, bus, truck
const std::vector<int> VEHICLE_CLASS_IDS_COCO = {2, 3, 5, 7};

ProcessingWorker::ProcessingWorker(QObject *parent) : QObject(parent) {
    nextVehicleID.fill(0);
}

ProcessingWorker::~ProcessingWorker() {}

bool ProcessingWorker::initializeModels(const QString& yoloModelPath, const QString& cocoNamesPath) {
    try {
        QString fullModelPath = QCoreApplication::applicationDirPath() + "/" + yoloModelPath;
        QString fullClassesPath = QCoreApplication::applicationDirPath() + "/" + cocoNamesPath;

        if (!QFileInfo::exists(fullModelPath)) {
            emit logMessage("YOLO model file not found at: " + fullModelPath, "ERROR");
            return false;
        }

        yoloNet = cv::dnn::readNetFromONNX(fullModelPath.toStdString());
        yoloNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        yoloNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        QFile file(fullClassesPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit logMessage("Could not open COCO names file: " + fullClassesPath, "ERROR");
            return false;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            classNames.push_back(in.readLine().trimmed().toStdString());
        }
        yoloInitialized = true;
        emit logMessage("YOLO model loaded successfully.", "INFO");
        return true;

    } catch (const cv::Exception& e) {
        emit logMessage(QString("OpenCV Exception during YOLO init: %1").arg(e.what()), "ERROR");
        return false;
    }
}

void ProcessingWorker::setYoloThresholds(float confidence, float nms) {
    yoloConfidenceThreshold = confidence;
    yoloNmsThreshold = nms;
}

void ProcessingWorker::processFrame(int roadIndex, cv::Mat frame, cv::Rect roi, TrafficLight currentLight) {
    if (frame.empty() || !yoloInitialized) return;

    cv::Mat processingFrame = frame;
    // Use the Region of Interest if it's valid
    if (roi.area() > 0) {
        processingFrame = frame(roi & cv::Rect(0, 0, frame.cols, frame.rows));
    }

    detectAndTrack(roadIndex, processingFrame, currentLight);
    drawDetections(frame, roadIndex);

    std::vector<int> violatingIDs;
    for(const auto& pair : roadTrackers[roadIndex]){
        // A vehicle is violating if it's a candidate for several frames, confirming movement on red.
        if(pair.second.isViolationCandidate && pair.second.violationFrameCount > 15) { // Threshold of ~0.5 seconds of detection
            violatingIDs.push_back(pair.first);
        }
    }

    emit processingFinished(roadIndex, matToQImage(frame), static_cast<int>(roadTrackers[roadIndex].size()), violatingIDs);
}

void ProcessingWorker::detectAndTrack(int roadIndex, cv::Mat &frame, TrafficLight currentLight) {
    std::vector<cv::Rect> detections = detectVehiclesYOLO(frame);
    updateTrackers(roadIndex, detections, currentLight);
}

// ===================================================================================
//
// THIS IS THE CORRECTED YOLOv8 PARSING LOGIC
//
// ===================================================================================
std::vector<cv::Rect> ProcessingWorker::detectVehiclesYOLO(const cv::Mat& frame) {
    std::vector<cv::Rect> boxes;
    if (!yoloInitialized) return boxes;

    try {
        cv::Mat blob;
        cv::dnn::blobFromImage(frame, blob, 1.0 / 255.0, cv::Size(640, 640), cv::Scalar(), true, false);
        yoloNet.setInput(blob);

        std::vector<cv::Mat> outputs;
        yoloNet.forward(outputs, yoloNet.getUnconnectedOutLayersNames());

        // The output of YOLOv8 is [1][84][8400]. We need to transpose it to [1][8400][84]
        cv::Mat detection_matrix = outputs[0].reshape(1, {outputs[0].size[1], outputs[0].size[2]});
        cv::transpose(detection_matrix, detection_matrix);


        std::vector<int> class_ids;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes_vec;

        float x_factor = static_cast<float>(frame.cols) / 640.0f;
        float y_factor = static_cast<float>(frame.rows) / 640.0f;

        for (int i = 0; i < detection_matrix.rows; i++) {
            float* row = detection_matrix.ptr<float>(i);
            cv::Mat class_scores = cv::Mat(1, classNames.size(), CV_32F, row + 4);

            cv::Point class_id_point;
            double max_class_score;
            cv::minMaxLoc(class_scores, 0, &max_class_score, 0, &class_id_point);

            if (max_class_score > yoloConfidenceThreshold) {
                // Check if the detected class is a vehicle
                if (std::find(VEHICLE_CLASS_IDS_COCO.begin(), VEHICLE_CLASS_IDS_COCO.end(), class_id_point.x) == VEHICLE_CLASS_IDS_COCO.end()){
                    continue;
                }

                confidences.push_back(static_cast<float>(max_class_score));
                class_ids.push_back(class_id_point.x);

                float cx = row[0];
                float cy = row[1];
                float w = row[2];
                float h = row[3];

                int left = static_cast<int>((cx - 0.5 * w) * x_factor);
                int top = static_cast<int>((cy - 0.5 * h) * y_factor);
                int width = static_cast<int>(w * x_factor);
                int height = static_cast<int>(h * y_factor);
                boxes_vec.push_back(cv::Rect(left, top, width, height));
            }
        }

        std::vector<int> nms_indices;
        cv::dnn::NMSBoxes(boxes_vec, confidences, yoloConfidenceThreshold, yoloNmsThreshold, nms_indices);
        for (int idx : nms_indices) {
            boxes.push_back(boxes_vec[idx]);
        }
    } catch (const cv::Exception& e) {
        emit logMessage(QString("YOLO detection cv::Exception: %1").arg(e.what()), "ERROR");
    }
    return boxes;
}

void ProcessingWorker::updateTrackers(int roadIndex, const std::vector<cv::Rect>& detections, TrafficLight currentLight) {
    auto& trackers = roadTrackers[roadIndex];
    const int maxFramesDisappeared = 15;

    std::vector<bool> usedDetections(detections.size(), false);

    // Update existing trackers based on IoU (Intersection over Union)
    for (auto& pair : trackers) {
        TrackedVehicle& tracker = pair.second;
        tracker.framesWithoutDetection++;
        int bestDetIdx = -1;
        double maxIoU = 0.0;

        for (size_t i = 0; i < detections.size(); ++i) {
            if (!usedDetections[i]) {
                double iou = (double)(tracker.boundingBox & detections[i]).area() / (double)(tracker.boundingBox | detections[i]).area();
                if (iou > maxIoU) {
                    maxIoU = iou;
                    bestDetIdx = i;
                }
            }
        }

        if (maxIoU > 0.3) {
            tracker.boundingBox = detections[bestDetIdx];
            tracker.framesWithoutDetection = 0;
            usedDetections[bestDetIdx] = true;

            // Violation logic: If light is red, vehicle is a candidate.
            if (currentLight == TrafficLight::RED) {
                tracker.violationFrameCount++;
                tracker.isViolationCandidate = true;
            } else { // Reset if light is not red
                tracker.violationFrameCount = 0;
                tracker.isViolationCandidate = false;
            }
        }
    }

    // Deregister old trackers that have been lost for too long
    for (auto it = trackers.begin(); it != trackers.end();) {
        if (it->second.framesWithoutDetection > maxFramesDisappeared) {
            it = trackers.erase(it);
        } else {
            ++it;
        }
    }

    // Register new detections as new trackers
    for (size_t i = 0; i < detections.size(); ++i) {
        if (!usedDetections[i]) {
            TrackedVehicle newVehicle;
            newVehicle.id = nextVehicleID[roadIndex]++;
            newVehicle.boundingBox = detections[i];
            trackers[newVehicle.id] = newVehicle;
        }
    }
}


void ProcessingWorker::drawDetections(cv::Mat &frame, int roadIndex) {
    const auto& trackers = roadTrackers[roadIndex];
    for (const auto& pair : trackers) {
        const TrackedVehicle& veh = pair.second;
        cv::Scalar color = veh.isViolationCandidate ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0); // Red if violation candidate
        cv::rectangle(frame, veh.boundingBox, color, 2);
        QString label = "ID: " + QString::number(veh.id);
        cv::putText(frame, label.toStdString(), cv::Point(veh.boundingBox.x, veh.boundingBox.y - 10), cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2);
    }
}

QImage ProcessingWorker::matToQImage(const cv::Mat& mat) {
    if (mat.empty()) return QImage();
    if (mat.type() == CV_8UC3) return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_RGB888).rgbSwapped();
    return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8);
}
