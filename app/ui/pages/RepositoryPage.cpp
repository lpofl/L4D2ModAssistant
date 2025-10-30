#include "app/ui/pages/RepositoryPage.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>

#include "app/ui/components/ModFilterPanel.h"
#include "app/ui/components/ModTableWidget.h"

RepositoryPage::RepositoryPage(QWidget* parent) : QWidget(parent) {
  buildUi();
  wireSignals();
}

void RepositoryPage::buildUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* filterRow = new QHBoxLayout();
  filterPanel_ = new ModFilterPanel(this);

  importBtn_ = new QPushButton(tr("导入"), this);
  importFolderBtn_ = new QPushButton(tr("导入文件夹"), this);
  filterRow->addWidget(new QLabel(tr("筛选项:"), this));
  filterRow->addWidget(filterPanel_, 1);

  showDeletedCheckBox_ = new QCheckBox(tr("显示已删除"), this);
  filterRow->addWidget(showDeletedCheckBox_);

  filterRow->addStretch(1);
  // 在导入按钮区域加入批量导入入口
  filterRow->addWidget(importFolderBtn_);
  filterRow->addWidget(importBtn_);
  layout->addLayout(filterRow);

  auto* splitter = new QSplitter(Qt::Horizontal, this);

  auto* leftPanel = new QWidget(splitter);
  auto* leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(8);

  modTable_ = new ModTableWidget(leftPanel);
  modTable_->configureColumns({tr("名称"),
                               tr("作者"),
                               tr("标签"),
                               tr("评分"),
                               tr("分类"),
                               tr("状态"),
                               tr("最后发布"),
                               tr("最后保存"),
                               tr("平台"),
                               tr("大小"),
                               tr("安全性"),
                               tr("稳定性"),
                               tr("获取方式"),
                               tr("备注")});

  leftLayout->addWidget(modTable_, 1);

  auto* actionRow = new QHBoxLayout();
  editBtn_ = new QPushButton(tr("编辑"), leftPanel);
  deleteBtn_ = new QPushButton(tr("删除"), leftPanel);
  refreshBtn_ = new QPushButton(tr("刷新"), leftPanel);
  actionRow->addWidget(editBtn_);
  actionRow->addWidget(deleteBtn_);
  actionRow->addStretch();
  actionRow->addWidget(refreshBtn_);
  leftLayout->addLayout(actionRow);

  leftPanel->setLayout(leftLayout);

  auto* rightPanel = new QWidget(splitter);
  auto* rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(12);

  coverLabel_ = new QLabel(tr("当前 MOD 图片"), rightPanel);
  coverLabel_->setAlignment(Qt::AlignCenter);
  coverLabel_->setMinimumSize(280, 240);
  coverLabel_->setStyleSheet("QLabel { background: #1f5f7f; color: white; border-radius: 6px; }");

  metaLabel_ = new QLabel(rightPanel);
  metaLabel_->setWordWrap(true);

  noteView_ = new QTextEdit(rightPanel);
  noteView_->setReadOnly(true);
  noteView_->setPlaceholderText(tr("当前 MOD 备注"));

  rightLayout->addWidget(coverLabel_);
  rightLayout->addWidget(metaLabel_);
  rightLayout->addWidget(noteView_, 1);
  rightPanel->setLayout(rightLayout);

  splitter->addWidget(leftPanel);
  splitter->addWidget(rightPanel);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);

  layout->addWidget(splitter, 1);
  setLayout(layout);
}

void RepositoryPage::wireSignals() {
  if (filterPanel_) {
    connect(filterPanel_, &ModFilterPanel::attributeChanged, this, &RepositoryPage::filterAttributeChanged);
    connect(filterPanel_, &ModFilterPanel::valueChanged, this, &RepositoryPage::filterValueChanged);
    connect(filterPanel_, &ModFilterPanel::valueTextChanged, this, &RepositoryPage::filterValueTextChanged);
  }
  if (modTable_) {
    connect(modTable_, &ModTableWidget::currentCellChanged, this, &RepositoryPage::currentCellChanged);
  }
  if (importBtn_) {
    connect(importBtn_, &QPushButton::clicked, this, &RepositoryPage::importRequested);
  }
  if (importFolderBtn_) {
    // 批量导入按钮触发新对话框
    connect(importFolderBtn_, &QPushButton::clicked, this, &RepositoryPage::importFolderRequested);
  }
  if (editBtn_) {
    connect(editBtn_, &QPushButton::clicked, this, &RepositoryPage::editRequested);
  }
  if (deleteBtn_) {
    connect(deleteBtn_, &QPushButton::clicked, this, &RepositoryPage::deleteRequested);
  }
  if (refreshBtn_) {
    connect(refreshBtn_, &QPushButton::clicked, this, &RepositoryPage::refreshRequested);
  }
  if (showDeletedCheckBox_) {
    connect(showDeletedCheckBox_, &QCheckBox::toggled, this, &RepositoryPage::showDeletedToggled);
  }
}
