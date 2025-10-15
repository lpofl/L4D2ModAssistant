
#include <QApplication>
#include "app/ui/MainWindow.h"

/**
 * @file main.cpp
 * @brief Qt 应用程序入口，创建并显示主窗口。
 */

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}
