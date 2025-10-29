#include "app/ui/pages/SettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "core/config/Settings.h"

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(0);

  settingsNav_ = new QListWidget(this);
  settingsNav_->setSelectionMode(QAbstractItemView::SingleSelection);
  settingsNav_->setFixedWidth(180);
  settingsNav_->addItem(tr("基础设置"));
  settingsNav_->addItem(tr("类别管理"));
  settingsNav_->addItem(tr("标签管理"));
  settingsNav_->addItem(tr("删除策略"));
  layout->addWidget(settingsNav_);

  auto* divider = new QFrame(this);
  divider->setFrameShape(QFrame::VLine);
  divider->setFrameShadow(QFrame::Sunken);
  divider->setFixedWidth(1);
  layout->addWidget(divider);

  settingsStack_ = new QStackedWidget(this);
  settingsStack_->addWidget(buildBasicSettingsPane());
  settingsStack_->addWidget(buildCategoryManagementPane());
  settingsStack_->addWidget(buildTagManagementPane());
  settingsStack_->addWidget(buildDeletionPane());
  layout->addWidget(settingsStack_, 1);

  settingsNav_->setCurrentRow(0);
}

QWidget* SettingsPage::buildBasicSettingsPane() {
  auto* container = new QWidget(this);
  auto* layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(16);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  form->setSpacing(12);

  settingsRepoDirEdit_ = new QLineEdit(container);
  settingsRepoDirEdit_->setPlaceholderText(tr("请选择本地仓库目录"));
  settingsRepoBrowseBtn_ = new QPushButton(tr("浏览..."), container);
  auto* repoRow = new QHBoxLayout();
  repoRow->setContentsMargins(0, 0, 0, 0);
  repoRow->setSpacing(8);
  repoRow->addWidget(settingsRepoDirEdit_, 1);
  repoRow->addWidget(settingsRepoBrowseBtn_);
  auto* repoWrapper = new QWidget(container);
  repoWrapper->setLayout(repoRow);
  form->addRow(tr("仓库目录"), repoWrapper);

  settingsGameDirEdit_ = new QLineEdit(container);
  settingsGameDirEdit_->setPlaceholderText(tr("请选择正确的 L4D2 游戏根目录"));
  settingsGameDirBrowseBtn_ = new QPushButton(tr("浏览..."), container);
  auto* gameRow = new QHBoxLayout();
  gameRow->setContentsMargins(0, 0, 0, 0);
  gameRow->setSpacing(8);
  gameRow->addWidget(settingsGameDirEdit_, 1);
  gameRow->addWidget(settingsGameDirBrowseBtn_);
  auto* gameWrapper = new QWidget(container);
  gameWrapper->setLayout(gameRow);
  form->addRow(tr("游戏根目录"), gameWrapper);

  settingsAddonsPathDisplay_ = new QLineEdit(container);
  settingsAddonsPathDisplay_->setReadOnly(true);
  settingsAddonsPathDisplay_->setPlaceholderText(tr("自动推导的 addons 目录"));
  form->addRow(tr("addons 目录"), settingsAddonsPathDisplay_);

  settingsWorkshopPathDisplay_ = new QLineEdit(container);
  settingsWorkshopPathDisplay_->setReadOnly(true);
  settingsWorkshopPathDisplay_->setPlaceholderText(tr("自动推导的 workshop 目录"));
  form->addRow(tr("workshop 目录"), settingsWorkshopPathDisplay_);

  importModeCombo_ = new QComboBox(container);
  importModeCombo_->addItem(tr("移动到仓库目录"), static_cast<int>(ImportAction::Cut));
  importModeCombo_->addItem(tr("复制到仓库目录"), static_cast<int>(ImportAction::Copy));
  importModeCombo_->addItem(tr("不处理"), static_cast<int>(ImportAction::None));
  form->addRow(tr("入库方式"), importModeCombo_);

  autoImportCheckbox_ = new QCheckBox(tr("自动整理游戏目录下的 addons"), container);
  form->addRow(QString(), autoImportCheckbox_);

  autoImportModeCombo_ = new QComboBox(container);
  autoImportModeCombo_->addItem(tr("移动到仓库目录"), static_cast<int>(AddonsAutoImportMethod::Cut));
  autoImportModeCombo_->addItem(tr("复制到仓库目录"), static_cast<int>(AddonsAutoImportMethod::Copy));
  autoImportModeCombo_->addItem(tr("建立链接"), static_cast<int>(AddonsAutoImportMethod::Link));
  form->addRow(tr("自动整理策略"), autoImportModeCombo_);

  layout->addLayout(form);

  settingsStatusLabel_ = new QLabel(container);
  settingsStatusLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  saveSettingsBtn_ = new QPushButton(tr("保存设置"), container);
  auto* actionRow = new QHBoxLayout();
  actionRow->setContentsMargins(0, 0, 0, 0);
  actionRow->setSpacing(12);
  actionRow->addWidget(settingsStatusLabel_, 1);
  actionRow->addWidget(saveSettingsBtn_);
  layout->addLayout(actionRow);

  layout->addStretch(1);
  return container;
}

QWidget* SettingsPage::buildCategoryManagementPane() {
  auto* container = new QWidget(this);
  auto* layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  categoryTree_ = new QTreeWidget(container);
  categoryTree_->setColumnCount(2);
  categoryTree_->setHeaderLabels({tr("名称"), tr("优先级")});
  categoryTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  categoryTree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  categoryTree_->setSelectionMode(QAbstractItemView::SingleSelection);
  categoryTree_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
  layout->addWidget(categoryTree_, 1);

  auto* buttonRow = new QHBoxLayout();
  buttonRow->setContentsMargins(0, 0, 0, 0);
  buttonRow->setSpacing(8);
  categoryAddRootBtn_ = new QPushButton(tr("新增根类别"), container);
  categoryAddChildBtn_ = new QPushButton(tr("新增子类别"), container);
  categoryRenameBtn_ = new QPushButton(tr("重命名"), container);
  categoryDeleteBtn_ = new QPushButton(tr("删除"), container);
  categoryMoveUpBtn_ = new QPushButton(tr("上移"), container);
  categoryMoveDownBtn_ = new QPushButton(tr("下移"), container);
  buttonRow->addWidget(categoryAddRootBtn_);
  buttonRow->addWidget(categoryAddChildBtn_);
  buttonRow->addWidget(categoryRenameBtn_);
  buttonRow->addWidget(categoryDeleteBtn_);
  buttonRow->addWidget(categoryMoveUpBtn_);
  buttonRow->addWidget(categoryMoveDownBtn_);
  buttonRow->addStretch(1);
  layout->addLayout(buttonRow);

  return container;
}

QWidget* SettingsPage::buildTagManagementPane() {
  auto* container = new QWidget(this);
  auto* layout = new QHBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  auto* groupPanel = new QWidget(container);
  auto* groupLayout = new QVBoxLayout(groupPanel);
  groupLayout->setContentsMargins(0, 0, 0, 0);
  groupLayout->setSpacing(8);

  groupLayout->addWidget(new QLabel(tr("标签组"), groupPanel));
  tagGroupList_ = new QListWidget(groupPanel);
  groupLayout->addWidget(tagGroupList_, 1);

  auto* groupButtonRow = new QHBoxLayout();
  groupButtonRow->setContentsMargins(0, 0, 0, 0);
  groupButtonRow->setSpacing(8);
  tagGroupAddBtn_ = new QPushButton(tr("新增组"), groupPanel);
  tagGroupRenameBtn_ = new QPushButton(tr("重命名"), groupPanel);
  tagGroupDeleteBtn_ = new QPushButton(tr("删除"), groupPanel);
  groupButtonRow->addWidget(tagGroupAddBtn_);
  groupButtonRow->addWidget(tagGroupRenameBtn_);
  groupButtonRow->addWidget(tagGroupDeleteBtn_);
  groupLayout->addLayout(groupButtonRow);

  auto* tagPanel = new QWidget(container);
  auto* tagLayout = new QVBoxLayout(tagPanel);
  tagLayout->setContentsMargins(0, 0, 0, 0);
  tagLayout->setSpacing(8);

  tagLayout->addWidget(new QLabel(tr("标签"), tagPanel));
  tagList_ = new QListWidget(tagPanel);
  tagLayout->addWidget(tagList_, 1);

  auto* tagButtonRow = new QHBoxLayout();
  tagButtonRow->setContentsMargins(0, 0, 0, 0);
  tagButtonRow->setSpacing(8);
  tagAddBtn_ = new QPushButton(tr("新增标签"), tagPanel);
  tagRenameBtn_ = new QPushButton(tr("重命名"), tagPanel);
  tagDeleteBtn_ = new QPushButton(tr("删除"), tagPanel);
  tagButtonRow->addWidget(tagAddBtn_);
  tagButtonRow->addWidget(tagRenameBtn_);
  tagButtonRow->addWidget(tagDeleteBtn_);
  tagLayout->addLayout(tagButtonRow);

  layout->addWidget(groupPanel, 1);
  layout->addWidget(tagPanel, 1);
  return container;
}

QWidget* SettingsPage::buildDeletionPane() {
  auto* container = new QWidget(this);
  auto* layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(16);

  retainDeletedCheckbox_ = new QCheckBox(tr("删除时保留源文件，便于还原"), container);
  layout->addWidget(retainDeletedCheckbox_);

  clearDeletedModsBtn_ = new QPushButton(tr("清理已删除的 MOD 记录"), container);
  layout->addWidget(clearDeletedModsBtn_, 0, Qt::AlignLeft);

  layout->addStretch(1);
  return container;
}
