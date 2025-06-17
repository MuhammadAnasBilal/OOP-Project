QT += core widgets gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# OpenCV 4.11.0 Configuration (Release mode only)
INCLUDEPATH += "C:/opencv/build/include"

# Release configuration for OpenCV
CONFIG(release, debug|release) {
    LIBS += -L"C:/opencv/build/x64/vc16/lib" \
            -lopencv_world4110
}

# Debug fallback (if needed)
CONFIG(debug, debug|release) {
    LIBS += -L"C:/opencv/build/x64/vc16/lib" \
            -lopencv_world4110d
}

TARGET = SmartTrafficSystem
TEMPLATE = app

# Source files
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    processingworker.cpp \
    trafficsystem.cpp

# Header files
HEADERS += \
    mainwindow.h \
    processingworker.h \
    traffic_types.h \
    trafficsystem.h
# Forms
FORMS += \
    mainwindow.ui

# Deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc \
    resources.qrc
