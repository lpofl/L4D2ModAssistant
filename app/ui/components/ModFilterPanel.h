#pragma once

#include <QWidget>

class QComboBox;
class QSortFilterProxyModel;
class QStandardItemModel;

class ModFilterPanel : public QWidget {
  Q_OBJECT
public:
  explicit ModFilterPanel(QWidget* parent = nullptr);

  QComboBox* attributeCombo() const { return attributeCombo_; }
  QComboBox* valueCombo() const { return valueCombo_; }

  void setAttributeItems(const QStringList& items);
  void setValueModels(QStandardItemModel* sourceModel, QSortFilterProxyModel* proxyModel);

signals:
  void attributeChanged(const QString& text);
  void valueChanged(const QString& text);
  void valueTextChanged(const QString& text);

private:
  void setupUi();
  void bindSignals();

  QComboBox* attributeCombo_{};
  QComboBox* valueCombo_{};
};

