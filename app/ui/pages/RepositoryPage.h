#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QTextEdit;

class ModFilterPanel;
class ModTableWidget;

class RepositoryPage : public QWidget {
  Q_OBJECT
public:
  explicit RepositoryPage(QWidget* parent = nullptr);

  ModFilterPanel* filterPanel() const { return filterPanel_; }
  QCheckBox* showDeletedCheckBox() const { return showDeletedCheckBox_; }
  ModTableWidget* modTable() const { return modTable_; }
  QPushButton* importButton() const { return importBtn_; }
  QPushButton* editButton() const { return editBtn_; }
  QPushButton* deleteButton() const { return deleteBtn_; }
  QPushButton* refreshButton() const { return refreshBtn_; }
  QLabel* coverLabel() const { return coverLabel_; }
  QLabel* metaLabel() const { return metaLabel_; }
  QTextEdit* noteView() const { return noteView_; }

signals:
  void filterAttributeChanged(const QString& text);
  void filterValueChanged(const QString& text);
  void filterValueTextChanged(const QString& text);
  void importRequested();
  void editRequested();
  void deleteRequested();
  void refreshRequested();
  void showDeletedToggled(bool checked);
  void currentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn);

private:
  void buildUi();
  void wireSignals();

  ModFilterPanel* filterPanel_{};
  QCheckBox* showDeletedCheckBox_{};
  ModTableWidget* modTable_{};
  QPushButton* importBtn_{};
  QPushButton* editBtn_{};
  QPushButton* deleteBtn_{};
  QPushButton* refreshBtn_{};
  QLabel* coverLabel_{};
  QLabel* metaLabel_{};
  QTextEdit* noteView_{};
};

