#include "app/ui/pages/SelectorPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "app/ui/components/ModFilterPanel.h"
#include "app/ui/components/ModTableWidget.h"

SelectorPage::SelectorPage(QWidget* parent) : QWidget(parent) {
  buildUi();
  wireSignals();
}

void SelectorPage::buildUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setSpacing(12);

  auto* filterRow = new QHBoxLayout();
  filterRow->setSpacing(8);

  filterPanel_ = new ModFilterPanel(this);
  filterRow->addWidget(new QLabel(tr("筛选项:"), this));
  filterRow->addWidget(filterPanel_, 1);
  layout->addLayout(filterRow);

  auto* tablesLayout = new QHBoxLayout();
  tablesLayout->setSpacing(12);

  // 左侧：游戏目录
  auto* leftPanel = new QWidget(this);
  auto* leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(8);

  auto* gameDirLabel = new QLabel(tr("游戏目录"), leftPanel);
  gameDirTable_ = new ModTableWidget(leftPanel);
  gameDirTable_->configureColumns({tr("名称"), tr("TAG"), tr("作者"), tr("评分"), tr("备注")});

  leftLayout->addWidget(gameDirLabel);
  leftLayout->addWidget(gameDirTable_);

  // 右侧：仓库数据
  auto* rightPanel = new QWidget(this);
  auto* rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(8);

  auto* repoLabel = new QLabel(tr("仓库"), rightPanel);
  repoTable_ = new ModTableWidget(rightPanel);
  repoTable_->configureColumns({tr("名称"), tr("TAG"), tr("作者"), tr("评分"), tr("备注")});

  rightLayout->addWidget(repoLabel);
  rightLayout->addWidget(repoTable_);

  tablesLayout->addWidget(leftPanel);
  tablesLayout->addWidget(rightPanel);
  layout->addLayout(tablesLayout);

  // 底部操作区域
  auto* buttonsLayout = new QHBoxLayout();
  buttonsLayout->setSpacing(12);

  configureStrategyBtn_ = new QPushButton(tr("配置策略"), this);
  randomizeBtn_ = new QPushButton(tr("随机一组"), this);
  saveCombinationBtn_ = new QPushButton(tr("保存组合"), this);
  applyToGameBtn_ = new QPushButton(tr("确认应用"), this);
  strategyInfoLabel_ = new QLabel(tr("已选策略信息"), this);

  buttonsLayout->addWidget(configureStrategyBtn_);
  buttonsLayout->addWidget(strategyInfoLabel_, 1);
  buttonsLayout->addStretch();
  buttonsLayout->addWidget(randomizeBtn_);
  buttonsLayout->addWidget(saveCombinationBtn_);
  buttonsLayout->addStretch();
  buttonsLayout->addWidget(applyToGameBtn_);

  layout->addLayout(buttonsLayout);
}

void SelectorPage::wireSignals() {
  if (filterPanel_) {
    connect(filterPanel_, &ModFilterPanel::attributeChanged, this, &SelectorPage::filterAttributeChanged);
    connect(filterPanel_, &ModFilterPanel::valueChanged, this, &SelectorPage::filterValueChanged);
    connect(filterPanel_, &ModFilterPanel::valueTextChanged, this, &SelectorPage::filterValueTextChanged);
  }
  connect(configureStrategyBtn_, &QPushButton::clicked, this, &SelectorPage::configureStrategyRequested);
  connect(randomizeBtn_, &QPushButton::clicked, this, &SelectorPage::randomizeRequested);
  connect(saveCombinationBtn_, &QPushButton::clicked, this, &SelectorPage::saveCombinationRequested);
  connect(applyToGameBtn_, &QPushButton::clicked, this, &SelectorPage::applyRequested);
}

