#include "app/ui/components/ModTableWidget.h"

#include <QAbstractItemView>
#include <QHeaderView>

ModTableWidget::ModTableWidget(QWidget* parent) : QTableWidget(parent) {
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setEditTriggers(QAbstractItemView::NoEditTriggers);
  setAlternatingRowColors(true);
  verticalHeader()->setVisible(false);
  setStyleSheet(
      "QTableWidget::item:selected {"
      " background-color: #D6EBFF;"
      " color: #1f3556;"
      " }"
      "QTableWidget::item:selected:!active {"
      " background-color: #E6F3FF;"
      " }");
}

void ModTableWidget::configureColumns(const QStringList& headers) {
  setColumnCount(headers.size());
  setHorizontalHeaderLabels(headers);
  if (auto* header = horizontalHeader()) {
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Stretch);
  }
}
