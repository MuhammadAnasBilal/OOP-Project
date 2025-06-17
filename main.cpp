#include "mainwindow.h"
#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QTimer>
#include <QPainter>
#include <QFont>
#include <QIcon> // Needed for QIcon

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);


    a.setWindowIcon(QIcon("C:\\Users\\Muhammad Anas Bilal\\Desktop\\STMS\\Images\\App_Logo.png"));

    // Create a pixmap for the splash screen
    QPixmap splashImage(800, 600);
    splashImage.fill(QColor("#2c3e50")); // Dark blue background

    // Load the logo from the local file system
    QPixmap logo("C:\\Users\\Muhammad Anas Bilal\\Desktop\\STMS\\Images\\Air_Logo.png");

    QPainter painter(&splashImage);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Draw the logo if it was loaded successfully
    if(!logo.isNull()) {
        int logoWidth = 200;
        int logoHeight = 200;
        // Center the logo horizontally, place it above the text
        painter.drawPixmap((splashImage.width() - logoWidth) / 2, 40, logoWidth, logoHeight, logo);
    }

    // Draw the title text
    painter.setPen(QColor(Qt::white));
    painter.setFont(QFont("Arial", 40, QFont::Bold));
    painter.drawText(splashImage.rect(), Qt::AlignCenter, "Smart Traffic\nManagement System");

    // Draw the "Loading..." text below
    painter.setFont(QFont("Arial", 14));
    painter.drawText(splashImage.rect().adjusted(0, 300, 0, 0), Qt::AlignCenter, "Loading...");
    painter.end();

    QSplashScreen splash(splashImage);
    splash.show();
    a.processEvents();

    MainWindow w;

    // Keep the splash screen visible for 3 seconds while the main window loads
    QTimer::singleShot(3000, &splash, &QWidget::close);
    QTimer::singleShot(3000, &w, &QWidget::show);

    return a.exec();
}


