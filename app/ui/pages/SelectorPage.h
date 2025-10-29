#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class ModFilterPanel;
class ModTableWidget;

/**
 * @brief 选择器页的 UI 容器，仅负责搭建界面并透出必要控件信号。
 */
class SelectorPage : public QWidget {
  Q_OBJECT
public:
  explicit SelectorPage(QWidget* parent = nullptr);

  ModFilterPanel* filterPanel() const { return filterPanel_; }
  ModTableWidget* gameDirectoryTable() const { return gameDirTable_; }
  ModTableWidget* repositoryTable() const { return repoTable_; }
  QPushButton* configureStrategyButton() const { return configureStrategyBtn_; }
  QPushButton* randomizeButton() const { return randomizeBtn_; }
  QPushButton* saveCombinationButton() const { return saveCombinationBtn_; }
  QPushButton* applyButton() const { return applyToGameBtn_; }
  QLabel* strategyInfoLabel() const { return strategyInfoLabel_; }

signals:
  void filterAttributeChanged(const QString& text);
  void filterValueChanged(const QString& text);
  void filterValueTextChanged(const QString& text);
  void configureStrategyRequested();
  void randomizeRequested();
  void saveCombinationRequested();
  void applyRequested();

private:
  void buildUi();
  void wireSignals();

  ModFilterPanel* filterPanel_{};
  ModTableWidget* gameDirTable_{};
  ModTableWidget* repoTable_{};
  QPushButton* configureStrategyBtn_{};
  QPushButton* randomizeBtn_{};
  QPushButton* saveCombinationBtn_{};
  QPushButton* applyToGameBtn_{};
  QLabel* strategyInfoLabel_{};
};

