#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include "app/ui/MainWindow.h"

/**
 * @file main.cpp
 * @brief Qt 应用程序入口，创建并显示主窗口。
 */

int main(int argc, char *argv[]) {
  QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
  QApplication app(argc, argv);

  QTranslator qtTranslator;
  const QString qtTranslations = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
  if (qtTranslator.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"), qtTranslations)) {
    app.installTranslator(&qtTranslator);
  }

  MainWindow w;
  w.show();
  return app.exec();
}
