#include "app/ui/components/NavigationBar.h"

#include <QHBoxLayout>
#include <QPushButton>

NavigationBar::NavigationBar(QWidget* parent) : QWidget(parent) {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  repoButton_ = new QPushButton(tr("仓库"), this);
  selectorButton_ = new QPushButton(tr("选择器"), this);
  settingsButton_ = new QPushButton(tr("设置"), this);

  layout->addWidget(repoButton_);
  layout->addWidget(selectorButton_);
  layout->addWidget(settingsButton_);
  layout->addStretch(1);

  connect(repoButton_, &QPushButton::clicked, this, &NavigationBar::repositoryRequested);
  connect(selectorButton_, &QPushButton::clicked, this, &NavigationBar::selectorRequested);
  connect(settingsButton_, &QPushButton::clicked, this, &NavigationBar::settingsRequested);

  setActive(Tab::Repository);
}

void NavigationBar::setActive(Tab tab) {
  switch (tab) {
    case Tab::Repository:
      updateButtonState(repoButton_);
      break;
    case Tab::Selector:
      updateButtonState(selectorButton_);
      break;
    case Tab::Settings:
      updateButtonState(settingsButton_);
      break;
  }
}

void NavigationBar::updateButtonState(QPushButton* active) {
  const QList<QPushButton*> buttons = {repoButton_, selectorButton_, settingsButton_};
  for (auto* button : buttons) {
    if (!button) continue;
    button->setCheckable(true);
    button->setChecked(button == active);
    button->setStyleSheet(button == active ? "QPushButton { background: #0f4a70; color: white; }"
                                           : "QPushButton { background: #d0e3ec; }");
  }
}

