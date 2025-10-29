#pragma once

#include <QWidget>

class QPushButton;

class NavigationBar : public QWidget {
  Q_OBJECT
public:
  enum class Tab {
    Repository,
    Selector,
    Settings,
  };

  explicit NavigationBar(QWidget* parent = nullptr);

  void setActive(Tab tab);

signals:
  void repositoryRequested();
  void selectorRequested();
  void settingsRequested();

private:
  void updateButtonState(QPushButton* active);

  QPushButton* repoButton_{};
  QPushButton* selectorButton_{};
  QPushButton* settingsButton_{};
};

