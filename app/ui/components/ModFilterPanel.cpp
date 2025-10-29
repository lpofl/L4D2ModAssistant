#include "app/ui/components/ModFilterPanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

ModFilterPanel::ModFilterPanel(QWidget* parent) : QWidget(parent) {
  setupUi();
  bindSignals();
}

void ModFilterPanel::setAttributeItems(const QStringList& items) {
  if (!attributeCombo_) return;
  attributeCombo_->clear();
  attributeCombo_->addItems(items);
}

void ModFilterPanel::setValueModels(QStandardItemModel* sourceModel, QSortFilterProxyModel* proxyModel) {
  if (!valueCombo_) return;

  if (proxyModel) {
    proxyModel->setSourceModel(sourceModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterKeyColumn(0);
    valueCombo_->setModel(proxyModel);
  } else if (sourceModel) {
    valueCombo_->setModel(sourceModel);
  } else {
    valueCombo_->setModel(nullptr);
  }
  valueCombo_->setCompleter(nullptr);
}

void ModFilterPanel::setupUi() {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  attributeCombo_ = new QComboBox(this);
  attributeCombo_->setEditable(false);

  valueCombo_ = new QComboBox(this);
  valueCombo_->setEditable(true);
  if (auto* edit = valueCombo_->lineEdit()) {
    edit->setClearButtonEnabled(true);
  }

  layout->addWidget(attributeCombo_, 1);
  layout->addWidget(valueCombo_, 2);
}

void ModFilterPanel::bindSignals() {
  if (!attributeCombo_ || !valueCombo_) return;

  connect(attributeCombo_, &QComboBox::currentTextChanged, this, &ModFilterPanel::attributeChanged);
  connect(valueCombo_, &QComboBox::currentTextChanged, this, &ModFilterPanel::valueChanged);
  connect(valueCombo_->lineEdit(), &QLineEdit::textChanged, this, &ModFilterPanel::valueTextChanged);
}

