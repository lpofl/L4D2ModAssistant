#pragma once

#include <QTableWidget>

class ModTableWidget : public QTableWidget {
  Q_OBJECT
public:
  explicit ModTableWidget(QWidget* parent = nullptr);

  void configureColumns(const QStringList& headers);
};

