#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include "trafficsystem.h"
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStartSystemClicked();
    void onStopSystemClicked();
    void onConnectCameraClicked(int roadIndex);
    void onDisconnectCameraClicked(int roadIndex);
    void onRefreshArduinoPortsClicked();
    void onApplyTrafficSettingsClicked();
    void onOpenViolationsFolderClicked();
    void onClearViolationsTableClicked();
    void onExportLogsClicked();
    void onClearLogClicked();
    void onArduinoPortSelected(const QString& portName);
    void onArduinoSimulationToggled(bool checked);
    void handleVehicleCountChanged(int roadIndex, int count);
    void handleDensityChanged(int roadIndex, TrafficDensity density);
    void handleTrafficLightChanged(int roadIndex, TrafficLight light);
    void handleViolationDetected(int roadIndex, const QString& timestamp, const QString& reason, const cv::Mat& frame);
    void handleFrameUpdated(int roadIndex, const QImage& frame);
    void handleCameraStatusChanged(int roadIndex, bool connected);
    void handleArduinoStatusChanged(bool connected, const QString& portName);
    void handleEnergySavingStatusChanged(bool active);
    void addLogMessage(const QString& message, const QString& level);


    void onUiUpdateTimerTimeout();

private:
    Ui::MainWindow *ui;
    TrafficSystem* trafficSystem;
    QTimer* uiUpdateTimer;
    void initializeUiConnections();
    void connectTrafficSystemSignals();
    void updateStatusbar();
    void populateArduinoPortsCombobox();
    void styleLightIndicatorLabel(QLabel* label, TrafficLight light);
    void addViolationEntryToTable(int roadIndex, const QString& timestamp, const QString& reason);
    QString formatDensityToString(TrafficDensity density) const;
};

#endif // MAINWINDOW_H
