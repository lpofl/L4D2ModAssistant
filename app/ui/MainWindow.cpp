#include "app/ui/MainWindow.h"

#include <algorithm>
#include <functional>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication> // For applicationDirPath()
#include <QDir>             // For mkpath()
#include <QSettings>        // For registry access
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMap>
#include <QStandardItem>
#include <QSet>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include "app/ui/ModEditorDialog.h"
#include "core/config/Settings.h"
#include "core/db/Db.h"
#include "core/db/Migrations.h"
#include "core/log/Log.h"

#include <QSortFilterProxyModel>
#include <QStandardItemModel>

namespace {

constexpr int kUncategorizedCategoryId = -1;
constexpr int kUntaggedTagId = -1;

QString joinTags(const std::vector<TagWithGroupRow>& tags) {
  QStringList parts;
  parts.reserve(static_cast<int>(tags.size()));
  for (const auto& tag : tags) {
    const QString group = QString::fromStdString(tag.group_name);
    const QString name = QString::fromStdString(tag.name);
    parts.push_back(group + u":" + name);
  }
  return parts.join(" / ");
}

QString toDisplay(const std::string& value, const QString& fallback = {}) {
  return value.empty() ? fallback : QString::fromStdString(value);
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setupUi();

  initLogging();
  settings_ = Settings::loadOrCreate();

  const QString appDirPath = QCoreApplication::applicationDirPath();
  const QString defaultRepoPath = QDir(appDirPath).filePath("mods");
  const QString legacyRepoPath = QDir(appDirPath).filePath("repo");
  const QString defaultDatabasePath = QDir(appDirPath).filePath("database/repo.db");

  bool settingsChanged = false;

  QString repoDirString = QString::fromStdString(settings_.repoDir);
  const QString cleanedLegacyRepo = QDir::cleanPath(legacyRepoPath);
  const QString cleanedDefaultRepo = QDir::cleanPath(defaultRepoPath);
  const bool usingLegacyRepo = !repoDirString.isEmpty() && QDir::cleanPath(repoDirString) == cleanedLegacyRepo;
  if (repoDirString.isEmpty() || usingLegacyRepo) {
    if (usingLegacyRepo) {
      QDir legacyDir(cleanedLegacyRepo);
      QDir newDir(cleanedDefaultRepo);
      if (legacyDir.exists() && !newDir.exists()) {
        if (!QDir().rename(cleanedLegacyRepo, cleanedDefaultRepo)) {
          spdlog::warn("Failed to rename legacy repo directory {} to {}", cleanedLegacyRepo.toStdString(), cleanedDefaultRepo.toStdString());
        }
      }
    }
    settings_.repoDir = cleanedDefaultRepo.toStdString();
    settingsChanged = true;
    spdlog::info("repoDir set to default mods directory: {}", settings_.repoDir);
  }

  settings_.repoDbPath = QDir::cleanPath(defaultDatabasePath).toStdString();

  QDir repoDir(QString::fromStdString(settings_.repoDir));
  if (!repoDir.exists()) {
    if (repoDir.mkpath(".")) {
      spdlog::info("Created repo directory: {}", settings_.repoDir);
      settingsChanged = true;
    } else {
      spdlog::error("Failed to create repo directory: {}", settings_.repoDir);
    }
  }

  const QString databaseDirPath = QDir(appDirPath).filePath("database");
  QDir databaseDir(databaseDirPath);
  if (!databaseDir.exists()) {
    if (!databaseDir.mkpath(".")) {
      spdlog::error("Failed to create database directory: {}", databaseDirPath.toStdString());
    }
  }

  // Detect L4D2 game directory if empty
  if (settings_.gameDirectory.empty()) {
    QString detectedPath = detectL4D2GameDirectory();
    if (!detectedPath.isEmpty()) {
      settings_.gameDirectory = detectedPath.toStdString();
      settingsChanged = true;
      spdlog::info("Detected L4D2 game directory: {}", settings_.gameDirectory);
    } else {
      spdlog::warn("Could not detect L4D2 game directory automatically.");
    }
  }

  if (settingsChanged) {
    settings_.save();
  }

  reinitializeRepository(settings_);
  refreshBasicSettingsUi();
  refreshCategoryManagementUi();
  refreshTagManagementUi();
  refreshDeletionSettingsUi();
}

void MainWindow::setupUi() {
  auto* central = new QWidget(this);
  auto* rootLayout = new QVBoxLayout(central);
  rootLayout->setContentsMargins(12, 12, 12, 12);
  rootLayout->setSpacing(12);

  rootLayout->addWidget(buildNavigationBar());

  stack_ = new QStackedWidget(central);
  stack_->addWidget(buildRepositoryPage());
  stack_->addWidget(buildSelectorPage());
  stack_->addWidget(buildSettingsPage());
  rootLayout->addWidget(stack_, 1);

  setCentralWidget(central);
  resize(1280, 760);
  setWindowTitle(QStringLiteral("L4D2 MOD 助手"));

  filterModel_ = new QStandardItemModel(this);
  proxyModel_ = new QSortFilterProxyModel(this);
  proxyModel_->setSourceModel(filterModel_);
  proxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
  proxyModel_->setFilterKeyColumn(0);
  filterValue_->setModel(proxyModel_);
  filterValue_->setCompleter(nullptr);

  selectorFilterModel_ = new QStandardItemModel(this);
  selectorProxyModel_ = new QSortFilterProxyModel(this);
  selectorProxyModel_->setSourceModel(selectorFilterModel_);
  selectorProxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
  selectorProxyModel_->setFilterKeyColumn(0);
  if (selectorFilterValue_) {
    selectorFilterValue_->setModel(selectorProxyModel_);
    selectorFilterValue_->setCompleter(nullptr);
  }

  filterAttribute_->addItems({tr("名称"), tr("分类"), tr("标签"), tr("作者"), tr("评分")});
  filterAttribute_->setCurrentText(tr("名称"));

  if (selectorFilterAttribute_) {
    selectorFilterAttribute_->addItems({tr("名称"), tr("分类"), tr("标签"), tr("作者"), tr("评分")});
    selectorFilterAttribute_->setCurrentText(tr("分类"));
  }
}

QWidget* MainWindow::buildNavigationBar() {
  auto* bar = new QWidget(this);
  auto* layout = new QHBoxLayout(bar);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  repoButton_ = new QPushButton(tr("仓库"), bar);
  selectorButton_ = new QPushButton(tr("选择器"), bar);
  settingsButton_ = new QPushButton(tr("设置"), bar);

  layout->addWidget(repoButton_);
  layout->addWidget(selectorButton_);
  layout->addWidget(settingsButton_);
  layout->addStretch();

  connect(repoButton_, &QPushButton::clicked, this, &MainWindow::switchToRepository);
  connect(selectorButton_, &QPushButton::clicked, this, &MainWindow::switchToSelector);
  connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::switchToSettings);

  updateTabButtonState(repoButton_);
  return bar;
}

QWidget* MainWindow::buildRepositoryPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* filterRow = new QHBoxLayout();
  filterAttribute_ = new QComboBox(page);
  filterValue_ = new QComboBox(page);
  filterValue_->setEditable(true);
  filterValue_->lineEdit()->setClearButtonEnabled(true);

  importBtn_ = new QPushButton(tr("导入"), page);
  filterRow->addWidget(new QLabel(tr("过滤器:"), page));
  filterRow->addWidget(filterAttribute_, 1);
  filterRow->addWidget(filterValue_, 2);

  showDeletedModsCheckBox_ = new QCheckBox(tr("显示已删除"), page);
  filterRow->addWidget(showDeletedModsCheckBox_);

  filterRow->addStretch(1);
  filterRow->addWidget(importBtn_);
  layout->addLayout(filterRow);

  auto* splitter = new QSplitter(Qt::Horizontal, page);

  auto* leftPanel = new QWidget(splitter);
  auto* leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(8);

  modTable_ = new QTableWidget(leftPanel);
  modTable_->setColumnCount(14);
  modTable_->setHorizontalHeaderLabels({tr("名称"),
                                        tr("分类"),
                                        tr("标签"),
                                        tr("作者"),
                                        tr("评分"),
                                        tr("状态"),
                                        tr("最后发布日"),
                                        tr("最后保存日"),
                                        tr("平台"),
                                        tr("链接"),
                                        tr("健全度"),
                                        tr("稳定性"),
                                        tr("获取方式"),
                                        tr("备注")});
  modTable_->horizontalHeader()->setStretchLastSection(true);
  modTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  modTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  modTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  modTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  modTable_->setAlternatingRowColors(true);
  modTable_->verticalHeader()->setVisible(false);
  modTable_->setStyleSheet(
      "QTableWidget::item:selected {"
      " background-color: #D6EBFF;"
      " color: #1f3556;"
      " }"
      "QTableWidget::item:selected:!active {"
      " background-color: #E6F3FF;"
      " }");

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

  connect(modTable_, &QTableWidget::currentCellChanged, this, &MainWindow::onCurrentRowChanged);
  connect(filterAttribute_, &QComboBox::currentTextChanged, this, &MainWindow::onFilterAttributeChanged);
  connect(filterValue_, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
  connect(filterValue_->lineEdit(), &QLineEdit::textChanged, this, &MainWindow::onFilterValueTextChanged);
  connect(importBtn_, &QPushButton::clicked, this, &MainWindow::onImport);
  connect(editBtn_, &QPushButton::clicked, this, &MainWindow::onEdit);
  connect(deleteBtn_, &QPushButton::clicked, this, &MainWindow::onDelete);
  connect(refreshBtn_, &QPushButton::clicked, this, &MainWindow::onRefresh);
  connect(showDeletedModsCheckBox_, &QCheckBox::toggled, this, &MainWindow::onShowDeletedModsToggled);

  return page;
}

QWidget* MainWindow::buildSelectorPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setSpacing(12);

  // 顶部过滤器行，与仓库页面保持一致的体验
  auto* filterRow = new QHBoxLayout();
  filterRow->setSpacing(8);
  filterRow->addWidget(new QLabel(tr("过滤器:"), page));
  selectorFilterAttribute_ = new QComboBox(page);
  selectorFilterValue_ = new QComboBox(page);
  selectorFilterValue_->setEditable(true);
  selectorFilterValue_->lineEdit()->setClearButtonEnabled(true);
  filterRow->addWidget(selectorFilterAttribute_, 1);
  filterRow->addWidget(selectorFilterValue_, 2);
  layout->addLayout(filterRow);

  auto* tablesLayout = new QHBoxLayout();
  tablesLayout->setSpacing(12);

  // Left panel (Game Directory)
  auto* leftPanel = new QWidget(page);
  auto* leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(8);

  auto* gameDirLabel = new QLabel(tr("游戏目录"), leftPanel);
  gameDirTable_ = new QTableWidget(leftPanel);

  gameDirTable_->setColumnCount(5);
  gameDirTable_->setHorizontalHeaderLabels({tr("名称"), tr("TAG"), tr("作者"), tr("评分"), tr("备注")});
  gameDirTable_->horizontalHeader()->setStretchLastSection(true);
  gameDirTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  gameDirTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  gameDirTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  gameDirTable_->setAlternatingRowColors(true);
  gameDirTable_->verticalHeader()->setVisible(false);

  leftLayout->addWidget(gameDirLabel);
  leftLayout->addWidget(gameDirTable_);

  // Right panel (Repository)
  auto* rightPanel = new QWidget(page);
  auto* rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(8);

  auto* repoLabel = new QLabel(tr("仓库"), rightPanel);
  repoTable_ = new QTableWidget(rightPanel);
  repoTable_->setColumnCount(5);
  repoTable_->setHorizontalHeaderLabels({tr("名称"), tr("TAG"), tr("作者"), tr("评分"), tr("备注")});
  repoTable_->horizontalHeader()->setStretchLastSection(true);
  repoTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  repoTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  repoTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  repoTable_->setAlternatingRowColors(true);
  repoTable_->verticalHeader()->setVisible(false);

  rightLayout->addWidget(repoLabel);
  rightLayout->addWidget(repoTable_);

  tablesLayout->addWidget(leftPanel);
  tablesLayout->addWidget(rightPanel);

  layout->addLayout(tablesLayout);

  // Bottom buttons
  auto* buttonsLayout = new QHBoxLayout();
  buttonsLayout->setSpacing(12);

  configureStrategyBtn_ = new QPushButton(tr("配置策略"), page);
  randomizeBtn_ = new QPushButton(tr("随机一组"), page);
  saveCombinationBtn_ = new QPushButton(tr("保存组合"), page);
  applyToGameBtn_ = new QPushButton(tr("确认应用"), page);
  strategyInfoLabel_ = new QLabel(tr("已选策略信息"), page);

  buttonsLayout->addWidget(configureStrategyBtn_);
  buttonsLayout->addWidget(strategyInfoLabel_, 1);
  buttonsLayout->addStretch();
  buttonsLayout->addWidget(randomizeBtn_);
  buttonsLayout->addWidget(saveCombinationBtn_);
  buttonsLayout->addStretch();
  buttonsLayout->addWidget(applyToGameBtn_);

  layout->addLayout(buttonsLayout);

  connect(selectorFilterAttribute_, &QComboBox::currentTextChanged, this, &MainWindow::onSelectorFilterAttributeChanged);
  connect(selectorFilterValue_, &QComboBox::currentTextChanged, this, &MainWindow::onSelectorFilterChanged);
  connect(selectorFilterValue_->lineEdit(), &QLineEdit::textChanged, this, &MainWindow::onSelectorFilterValueTextChanged);

  connect(configureStrategyBtn_, &QPushButton::clicked, this, &MainWindow::onConfigureStrategy);
  connect(randomizeBtn_, &QPushButton::clicked, this, &MainWindow::onRandomize);
  connect(saveCombinationBtn_, &QPushButton::clicked, this, &MainWindow::onSaveCombination);
  connect(applyToGameBtn_, &QPushButton::clicked, this, &MainWindow::onApplyToGame);

  return page;
}

QWidget* MainWindow::buildSettingsPage() {
  auto* page = new QWidget(this);
  auto* layout = new QHBoxLayout(page);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(0);

  settingsNav_ = new QListWidget(page);
  settingsNav_->setSelectionMode(QAbstractItemView::SingleSelection);
  settingsNav_->setFixedWidth(180);
  settingsNav_->addItem(tr("基础设置"));
  settingsNav_->addItem(tr("分类管理"));
  settingsNav_->addItem(tr("标签管理"));
  settingsNav_->addItem(tr("删除管理"));
  layout->addWidget(settingsNav_);

  auto* divider = new QFrame(page);
  divider->setFrameShape(QFrame::VLine);
  divider->setFrameShadow(QFrame::Sunken);
  divider->setLineWidth(1);
  divider->setMidLineWidth(0);
  layout->addWidget(divider);

  settingsStack_ = new QStackedWidget(page);
  auto wrapScroll = [](QWidget* content) {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
  };
  settingsStack_->addWidget(wrapScroll(buildBasicSettingsPane()));
  settingsStack_->addWidget(wrapScroll(buildCategoryManagementPane()));
  settingsStack_->addWidget(wrapScroll(buildTagManagementPane()));
  settingsStack_->addWidget(wrapScroll(buildDeletionPane()));
  layout->addWidget(settingsStack_, 1);

  connect(settingsNav_, &QListWidget::currentRowChanged, this, &MainWindow::onSettingsNavChanged);
  ensureSettingsNavSelection();

  return page;
}

QWidget* MainWindow::buildBasicSettingsPane() {
  auto* container = new QWidget(this);
  auto* layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(16);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
  form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  form->setSpacing(12);

  settingsRepoDirEdit_ = new QLineEdit(container);
  settingsRepoDirEdit_->setPlaceholderText(tr("选择或输入仓库目录"));
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
  settingsGameDirEdit_->setPlaceholderText(tr("选择或输入游戏目录"));
  settingsGameDirBrowseBtn_ = new QPushButton(tr("浏览..."), container);
  auto* gameRow = new QHBoxLayout();
  gameRow->setContentsMargins(0, 0, 0, 0);
  gameRow->setSpacing(8);
  gameRow->addWidget(settingsGameDirEdit_, 1);
  gameRow->addWidget(settingsGameDirBrowseBtn_);
  auto* gameWrapper = new QWidget(container);
  gameWrapper->setLayout(gameRow);
  form->addRow(tr("游戏目录"), gameWrapper);

  importModeCombo_ = new QComboBox(container);
  importModeCombo_->addItem(tr("剪切到仓库目录"), static_cast<int>(ImportAction::Cut));
  importModeCombo_->addItem(tr("复制到仓库目录"), static_cast<int>(ImportAction::Copy));
  importModeCombo_->addItem(tr("仅链接"), static_cast<int>(ImportAction::None));
  form->addRow(tr("导入方式"), importModeCombo_);

  autoImportCheckbox_ = new QCheckBox(tr("自动导入游戏目录下的 addons"), container);
  form->addRow(QString(), autoImportCheckbox_);

  autoImportModeCombo_ = new QComboBox(container);
  autoImportModeCombo_->addItem(tr("剪切到仓库目录"), static_cast<int>(AddonsAutoImportMethod::Cut));
  autoImportModeCombo_->addItem(tr("复制到仓库目录"), static_cast<int>(AddonsAutoImportMethod::Copy));
  autoImportModeCombo_->addItem(tr("仅链接"), static_cast<int>(AddonsAutoImportMethod::Link));
  form->addRow(tr("自动导入方式"), autoImportModeCombo_);

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

  connect(settingsRepoBrowseBtn_, &QPushButton::clicked, this, &MainWindow::onBrowseRepoDir);
  connect(settingsGameDirBrowseBtn_, &QPushButton::clicked, this, &MainWindow::onBrowseGameDir);
  connect(importModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onImportModeChanged);
  connect(autoImportCheckbox_, &QCheckBox::toggled, this, &MainWindow::onAutoImportToggled);
  connect(autoImportModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onAutoImportModeChanged);
  connect(saveSettingsBtn_, &QPushButton::clicked, this, &MainWindow::onSaveSettings);

  return container;
}

QWidget* MainWindow::buildCategoryManagementPane() {
  auto* container = new QWidget(this);
  auto* layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  categoryTree_ = new QTreeWidget(container);
  categoryTree_->setColumnCount(2);
  categoryTree_->setHeaderLabels({tr("分类"), tr("优先级")});
  categoryTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  categoryTree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  categoryTree_->setSelectionMode(QAbstractItemView::SingleSelection);
  categoryTree_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
  layout->addWidget(categoryTree_, 1);

  auto* buttonRow = new QHBoxLayout();
  buttonRow->setContentsMargins(0, 0, 0, 0);
  buttonRow->setSpacing(8);
  categoryAddRootBtn_ = new QPushButton(tr("新增一级分类"), container);
  categoryAddChildBtn_ = new QPushButton(tr("新增子分类"), container);
  categoryRenameBtn_ = new QPushButton(tr("重命名"), container);
  categoryDeleteBtn_ = new QPushButton(tr("删除"), container);
  categoryMoveUpBtn_ = new QPushButton(tr("上升"), container);
  categoryMoveDownBtn_ = new QPushButton(tr("下降"), container);
  buttonRow->addWidget(categoryAddRootBtn_);
  buttonRow->addWidget(categoryAddChildBtn_);
  buttonRow->addWidget(categoryRenameBtn_);
  buttonRow->addWidget(categoryDeleteBtn_);
  buttonRow->addWidget(categoryMoveUpBtn_);
  buttonRow->addWidget(categoryMoveDownBtn_);
  buttonRow->addStretch(1);
  layout->addLayout(buttonRow);

  connect(categoryTree_, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onCategorySelectionChanged);
  connect(categoryTree_, &QTreeWidget::itemChanged, this, &MainWindow::onCategoryItemChanged);
  connect(categoryAddRootBtn_, &QPushButton::clicked, this, &MainWindow::onAddCategoryTopLevel);
  connect(categoryAddChildBtn_, &QPushButton::clicked, this, &MainWindow::onAddCategoryChild);
  connect(categoryRenameBtn_, &QPushButton::clicked, this, &MainWindow::onRenameCategory);
  connect(categoryDeleteBtn_, &QPushButton::clicked, this, &MainWindow::onDeleteCategory);
  connect(categoryMoveUpBtn_, &QPushButton::clicked, this, &MainWindow::onMoveCategoryUp);
  connect(categoryMoveDownBtn_, &QPushButton::clicked, this, &MainWindow::onMoveCategoryDown);

  return container;
}

QWidget* MainWindow::buildTagManagementPane() {
  auto* container = new QWidget(this);
  auto* layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  auto* contentRow = new QHBoxLayout();
  contentRow->setContentsMargins(0, 0, 0, 0);
  contentRow->setSpacing(12);

  auto* groupPanel = new QVBoxLayout();
  groupPanel->setSpacing(8);
  tagGroupList_ = new QListWidget(container);
  tagGroupList_->setSelectionMode(QAbstractItemView::SingleSelection);
  groupPanel->addWidget(tagGroupList_, 1);
  auto* groupButtons = new QHBoxLayout();
  groupButtons->setSpacing(8);
  tagGroupAddBtn_ = new QPushButton(tr("新增组"), container);
  tagGroupRenameBtn_ = new QPushButton(tr("重命名"), container);
  tagGroupDeleteBtn_ = new QPushButton(tr("删除"), container);
  groupButtons->addWidget(tagGroupAddBtn_);
  groupButtons->addWidget(tagGroupRenameBtn_);
  groupButtons->addWidget(tagGroupDeleteBtn_);
  groupButtons->addStretch(1);
  groupPanel->addLayout(groupButtons);

  auto* tagPanel = new QVBoxLayout();
  tagPanel->setSpacing(8);
  tagList_ = new QListWidget(container);
  tagList_->setSelectionMode(QAbstractItemView::SingleSelection);
  tagPanel->addWidget(tagList_, 1);
  auto* tagButtons = new QHBoxLayout();
  tagButtons->setSpacing(8);
  tagAddBtn_ = new QPushButton(tr("新增标签"), container);
  tagRenameBtn_ = new QPushButton(tr("重命名"), container);
  tagDeleteBtn_ = new QPushButton(tr("删除"), container);
  tagButtons->addWidget(tagAddBtn_);
  tagButtons->addWidget(tagRenameBtn_);
  tagButtons->addWidget(tagDeleteBtn_);
  tagButtons->addStretch(1);
  tagPanel->addLayout(tagButtons);

  contentRow->addLayout(groupPanel, 1);
  contentRow->addLayout(tagPanel, 1);
  layout->addLayout(contentRow, 1);
  layout->addStretch(1);

  connect(tagGroupList_, &QListWidget::currentRowChanged, this, &MainWindow::onTagGroupSelectionChanged);
  connect(tagList_, &QListWidget::currentRowChanged, this, &MainWindow::onTagSelectionChanged);
  connect(tagGroupAddBtn_, &QPushButton::clicked, this, &MainWindow::onAddTagGroup);
  connect(tagGroupRenameBtn_, &QPushButton::clicked, this, &MainWindow::onRenameTagGroup);
  connect(tagGroupDeleteBtn_, &QPushButton::clicked, this, &MainWindow::onDeleteTagGroup);
  connect(tagAddBtn_, &QPushButton::clicked, this, &MainWindow::onAddTag);
  connect(tagRenameBtn_, &QPushButton::clicked, this, &MainWindow::onRenameTag);
  connect(tagDeleteBtn_, &QPushButton::clicked, this, &MainWindow::onDeleteTag);

  return container;
}

QWidget* MainWindow::buildDeletionPane() {
  auto* container = new QWidget(this);
  auto* layout = new QVBoxLayout(container);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(16);

  retainDeletedCheckbox_ = new QCheckBox(tr("删除 MOD 时保留数据记录"), container);
  layout->addWidget(retainDeletedCheckbox_);

  auto* noteLabel = new QLabel(tr("如果关闭此选项，删除 MOD 时会在数据库中完全移除记录。"), container);
  noteLabel->setWordWrap(true);
  layout->addWidget(noteLabel);

  clearDeletedModsBtn_ = new QPushButton(tr("清除已删除MOD数据记录"), container);
  layout->addWidget(clearDeletedModsBtn_);
  layout->addStretch(1);

  connect(clearDeletedModsBtn_, &QPushButton::clicked, this, &MainWindow::onClearDeletedMods);

  return container;
}

void MainWindow::refreshBasicSettingsUi() {
  if (!settingsRepoDirEdit_) {
    return;
  }

  {
    QSignalBlocker blocker(settingsRepoDirEdit_);
    settingsRepoDirEdit_->setText(QString::fromStdString(settings_.repoDir));
  }

  if (settingsGameDirEdit_) {
    QSignalBlocker blocker(settingsGameDirEdit_);
    settingsGameDirEdit_->setText(QString::fromStdString(settings_.gameDirectory));
  }

  if (importModeCombo_) {
    QSignalBlocker blocker(importModeCombo_);
    const int index = importModeCombo_->findData(static_cast<int>(settings_.importAction));
    importModeCombo_->setCurrentIndex(index >= 0 ? index : 0);
  }

  if (autoImportCheckbox_) {
    QSignalBlocker blocker(autoImportCheckbox_);
    autoImportCheckbox_->setChecked(settings_.addonsAutoImportEnabled);
  }

  if (autoImportModeCombo_) {
    QSignalBlocker blocker(autoImportModeCombo_);
    const int index = autoImportModeCombo_->findData(static_cast<int>(settings_.addonsAutoImportMethod));
    autoImportModeCombo_->setCurrentIndex(index >= 0 ? index : 0);
    autoImportModeCombo_->setEnabled(settings_.addonsAutoImportEnabled);
  }

  setSettingsStatus({});
}

void MainWindow::refreshCategoryManagementUi() {
  if (!categoryTree_ || !repo_) {
    return;
  }

  const int previousId = selectedCategoryId();
  QSignalBlocker blocker(categoryTree_);
  suppressCategoryItemSignals_ = true;
  categoryTree_->clear();

  const auto categories = repo_->listCategories();
  std::unordered_map<int, std::vector<CategoryRow>> children;
  std::vector<CategoryRow> roots;
  for (const auto& cat : categories) {
    if (cat.parent_id.has_value()) {
      children[*cat.parent_id].push_back(cat);
    } else {
      roots.push_back(cat);
    }
  }

  const auto compare = [](const CategoryRow& a, const CategoryRow& b) {
    if (a.priority != b.priority) return a.priority < b.priority;
    if (a.name != b.name) return a.name < b.name;
    return a.id < b.id;
  };

  std::sort(roots.begin(), roots.end(), compare);
  for (auto& entry : children) {
    auto& bucket = entry.second;
    std::sort(bucket.begin(), bucket.end(), compare);
  }

  std::function<void(const CategoryRow&, QTreeWidgetItem*)> addItem;
  addItem = [&](const CategoryRow& cat, QTreeWidgetItem* parent) {
    auto* item = new QTreeWidgetItem();
    const QString name = QString::fromStdString(cat.name);
    item->setText(0, name);
    item->setText(1, QString::number(cat.priority));
    item->setData(0, Qt::UserRole, cat.id);
    if (cat.parent_id.has_value()) {
      item->setData(0, Qt::UserRole + 1, cat.parent_id.value());
    }
    item->setData(0, Qt::UserRole + 2, name);
    item->setData(1, Qt::UserRole, cat.priority);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);

    if (parent) {
      parent->addChild(item);
    } else {
      categoryTree_->addTopLevelItem(item);
    }

    if (const auto it = children.find(cat.id); it != children.end()) {
      for (const auto& child : it->second) {
        addItem(child, item);
      }
    }
  };

  for (const auto& root : roots) {
    addItem(root, nullptr);
  }

  categoryTree_->expandAll();
  suppressCategoryItemSignals_ = false;

  if (previousId > 0) {
    QTreeWidgetItemIterator it(categoryTree_);
    while (*it) {
      if ((*it)->data(0, Qt::UserRole).toInt() == previousId) {
        categoryTree_->setCurrentItem(*it);
        break;
      }
      ++it;
    }
  }

  onCategorySelectionChanged();
}

void MainWindow::refreshTagManagementUi() {
  if (!tagGroupList_ || !repo_) {
    return;
  }

  const int previousGroup = selectedTagGroupId();
  QSignalBlocker blocker(tagGroupList_);
  tagGroupList_->clear();

  const auto groups = repo_->listTagGroups();
  for (const auto& group : groups) {
    auto* item = new QListWidgetItem(QString::fromStdString(group.name));
    item->setData(Qt::UserRole, group.id);
    tagGroupList_->addItem(item);
  }

  int rowToSelect = 0;
  if (previousGroup > 0) {
    for (int row = 0; row < tagGroupList_->count(); ++row) {
      if (tagGroupList_->item(row)->data(Qt::UserRole).toInt() == previousGroup) {
        rowToSelect = row;
        break;
      }
    }
  }

  if (tagGroupList_->count() > 0) {
    tagGroupList_->setCurrentRow(rowToSelect);
  } else {
    tagGroupList_->setCurrentRow(-1);
  }

  refreshTagListForGroup(selectedTagGroupId());
}

void MainWindow::refreshTagListForGroup(int groupId) {
  if (!tagList_) {
    return;
  }

  const int previousTag = selectedTagId();
  QSignalBlocker blocker(tagList_);
  tagList_->clear();

  if (groupId > 0 && repo_) {
    const auto tags = repo_->listTagsInGroup(groupId);
    for (const auto& tag : tags) {
      auto* item = new QListWidgetItem(QString::fromStdString(tag.name));
      item->setData(Qt::UserRole, tag.id);
      tagList_->addItem(item);
    }

    if (previousTag > 0) {
      for (int row = 0; row < tagList_->count(); ++row) {
        if (tagList_->item(row)->data(Qt::UserRole).toInt() == previousTag) {
          tagList_->setCurrentRow(row);
          break;
        }
      }
    } else if (tagList_->count() > 0) {
      tagList_->setCurrentRow(0);
    }
  }

  if (tagList_->count() == 0) {
    tagList_->setCurrentRow(-1);
  }

  const bool hasGroup = groupId > 0;
  if (tagGroupRenameBtn_) tagGroupRenameBtn_->setEnabled(hasGroup);
  if (tagGroupDeleteBtn_) tagGroupDeleteBtn_->setEnabled(hasGroup);
  if (tagAddBtn_) tagAddBtn_->setEnabled(hasGroup);

  onTagSelectionChanged(tagList_ ? tagList_->currentRow() : -1);
}

void MainWindow::refreshDeletionSettingsUi() {
  if (retainDeletedCheckbox_) {
    QSignalBlocker blocker(retainDeletedCheckbox_);
    retainDeletedCheckbox_->setChecked(settings_.retainDataOnDelete);
  }
}

void MainWindow::ensureSettingsNavSelection() {
  if (!settingsNav_ || settingsNav_->count() == 0) {
    return;
  }
  if (settingsNav_->currentRow() < 0) {
    settingsNav_->setCurrentRow(0);
  }
}

void MainWindow::setSettingsStatus(const QString& text, bool isError) {
  if (!settingsStatusLabel_) {
    return;
  }
  settingsStatusLabel_->setText(text);
  if (text.isEmpty()) {
    settingsStatusLabel_->setStyleSheet(QString());
  } else if (isError) {
    settingsStatusLabel_->setStyleSheet(QStringLiteral("color: #d9534f;"));
  } else {
    settingsStatusLabel_->setStyleSheet(QStringLiteral("color: #198754;"));
  }
}

void MainWindow::reinitializeRepository(const Settings& settings) {
  repoDir_ = QString::fromStdString(settings.repoDir);
  spdlog::info("Repo DB: {}", settings.repoDbPath);

  auto db = std::make_shared<Db>(settings.repoDbPath);
  runMigrations(*db);
  spdlog::info("Schema ready, version {}", migrations::currentSchemaVersion(*db));
  repo_ = std::make_unique<RepositoryService>(db);

  reloadCategories();
  loadData();
  reloadRepoSelectorData();

  // 初始化时主动触发一次过滤器刷新，确保搜索框保持空白以显示占位提示
  onFilterAttributeChanged(filterAttribute_->currentText());
}

int MainWindow::selectedCategoryId() const {
  if (!categoryTree_) {
    return 0;
  }
  if (auto* item = categoryTree_->currentItem()) {
    return item->data(0, Qt::UserRole).toInt();
  }
  return 0;
}

int MainWindow::selectedTagGroupId() const {
  if (!tagGroupList_) {
    return 0;
  }
  if (auto* item = tagGroupList_->currentItem()) {
    return item->data(Qt::UserRole).toInt();
  }
  return 0;
}

int MainWindow::selectedTagId() const {
  if (!tagList_) {
    return 0;
  }
  if (auto* item = tagList_->currentItem()) {
    return item->data(Qt::UserRole).toInt();
  }
  return 0;
}

void MainWindow::reloadCategories() {
  categoryNames_.clear();
  categoryParent_.clear();
  populateCategoryFilterModel(filterModel_, true);
}

void MainWindow::loadData() {
  // Always load all mods, including deleted ones, for client-side filtering.
  mods_ = repo_->listAll(true);
  modTagsText_.clear();
  modTagsCache_.clear();
  for (const auto& mod : mods_) {
    auto tagRows = repo_->listTagsForMod(mod.id);
    modTagsCache_[mod.id] = tagRows;
    modTagsText_[mod.id] = formatTagSummary(tagRows, QStringLiteral("  |  "), QStringLiteral(" / "));
  }
  populateTable();
  applySelectorFilter();
}

void MainWindow::populateTable() {
  modTable_->setRowCount(0);

  const QString filterAttribute = filterAttribute_->currentText();
  const QString filterValueText = filterValue_->currentText();
  const int filterId = filterIdForCombo(filterValue_, proxyModel_, filterModel_);

  int row = 0;
  for (const auto& mod : mods_) {
    // Filter by deleted status
    if (!showDeletedModsCheckBox_->isChecked() && mod.is_deleted) {
      continue;
    }

    if (!modMatchesFilter(mod, filterAttribute, filterId, filterValueText)) {
      continue;
    }

    const QString name = QString::fromStdString(mod.name);
    const QString author = toDisplay(mod.author);
    const QString status = toDisplay(mod.status, tr("最新"));
    const QString lastPublished = toDisplay(mod.last_published_at, tr("-"));
    const QString lastSaved = toDisplay(mod.last_saved_at, tr("-"));
    const QString platform = toDisplay(mod.source_platform);
    const QString url = toDisplay(mod.source_url);
    const QString note = toDisplay(mod.note);
    const QString integrity = toDisplay(mod.integrity);
    const QString stability = toDisplay(mod.stability);
    const QString acquisition = toDisplay(mod.acquisition_method);
    const QString tags = modTagsText_[mod.id];

    modTable_->insertRow(row);
    auto* itemName = new QTableWidgetItem(name);
    itemName->setData(Qt::UserRole, mod.id);
    modTable_->setItem(row, 0, itemName);
    modTable_->setItem(row, 1, new QTableWidgetItem(categoryNameFor(mod.category_id)));
    modTable_->setItem(row, 2, new QTableWidgetItem(tags));
    modTable_->setItem(row, 3, new QTableWidgetItem(author));
    modTable_->setItem(row, 4, new QTableWidgetItem(mod.rating > 0 ? QString::number(mod.rating) : QString("-")));
    modTable_->setItem(row, 5, new QTableWidgetItem(status));
    modTable_->setItem(row, 6, new QTableWidgetItem(lastPublished));
    modTable_->setItem(row, 7, new QTableWidgetItem(lastSaved));
    modTable_->setItem(row, 8, new QTableWidgetItem(platform));
    modTable_->setItem(row, 9, new QTableWidgetItem(url));
    modTable_->setItem(row, 10, new QTableWidgetItem(integrity.isEmpty() ? tr("-") : integrity));
    modTable_->setItem(row, 11, new QTableWidgetItem(stability.isEmpty() ? tr("-") : stability));
    modTable_->setItem(row, 12, new QTableWidgetItem(acquisition.isEmpty() ? tr("-") : acquisition));
    modTable_->setItem(row, 13, new QTableWidgetItem(note));
    ++row;
  }

  if (modTable_->rowCount() > 0) {
    modTable_->setCurrentCell(0, 0);
  } else {
    updateDetailForMod(-1);
  }
}

int MainWindow::filterIdForCombo(const QComboBox* combo,
                                 const QSortFilterProxyModel* proxy,
                                 const QStandardItemModel* model) const {
  if (!combo || !proxy || !model) {
    return 0;
  }
  const int index = combo->currentIndex();
  if (index < 0) {
    return 0;
  }
  const QModelIndex proxyIndex = proxy->index(index, 0);
  if (!proxyIndex.isValid()) {
    return 0;
  }
  const QModelIndex sourceIndex = proxy->mapToSource(proxyIndex);
  if (!sourceIndex.isValid()) {
    return 0;
  }
  return model->data(sourceIndex, Qt::UserRole).toInt();
}

bool MainWindow::modMatchesFilter(const ModRow& mod,
                                  const QString& attribute,
                                  int filterId,
                                  const QString& filterValue) const {
  if (attribute == tr("分类")) {
    if (filterId == kUncategorizedCategoryId) {
      return mod.category_id == 0;
    }
    if (filterId > 0) {
      return categoryMatchesFilter(mod.category_id, filterId);
    }
    return true;
  }

  if (attribute == tr("名称")) {
    if (filterValue.isEmpty()) {
      return true;
    }
    const QString name = QString::fromStdString(mod.name);
    return name.contains(filterValue, Qt::CaseInsensitive);
  }

  if (attribute == tr("标签")) {
    const auto tagsIt = modTagsCache_.find(mod.id);
    static const std::vector<TagWithGroupRow> kEmptyTags;
    const auto& modTags = tagsIt != modTagsCache_.end() ? tagsIt->second : kEmptyTags;

    if (filterId == kUntaggedTagId) {
      return modTags.empty();
    }
    if (filterId > 0) {
      const auto it = std::find_if(modTags.begin(), modTags.end(), [filterId](const TagWithGroupRow& tag) {
        return tag.id == filterId;
      });
      return it != modTags.end();
    }
    return true;
  }

  if (attribute == tr("作者")) {
    const QString authorFilter = filterValue.trimmed();
    if (authorFilter.isEmpty()) {
      return true;
    }
    return QString::fromStdString(mod.author) == authorFilter;
  }

  if (attribute == tr("评分")) {
    if (filterId == 0) {
      return true;
    }
    if (filterId > 0) {
      return mod.rating == filterId;
    }
    return mod.rating <= 0;
  }

  return true;
}

void MainWindow::applySelectorFilter() {
  if (!repoTable_ || !selectorFilterAttribute_) {
    return;
  }

  const QString attribute = selectorFilterAttribute_->currentText();
  const QString filterValueText = selectorFilterValue_ ? selectorFilterValue_->currentText() : QString();
  const int filterId = filterIdForCombo(selectorFilterValue_, selectorProxyModel_, selectorFilterModel_);

  repoTable_->setRowCount(0);
  int row = 0;
  for (const auto& mod : mods_) {
    if (mod.is_deleted) {
      continue;
    }
    if (!modMatchesFilter(mod, attribute, filterId, filterValueText)) {
      continue;
    }

    repoTable_->insertRow(row);
    auto* itemName = new QTableWidgetItem(QString::fromStdString(mod.name));
    itemName->setData(Qt::UserRole, mod.id);
    repoTable_->setItem(row, 0, itemName);
    const auto tagsIt = modTagsText_.find(mod.id);
    const QString tagsText = tagsIt != modTagsText_.end() ? tagsIt->second : QString();
    repoTable_->setItem(row, 1, new QTableWidgetItem(tagsText));
    repoTable_->setItem(row, 2, new QTableWidgetItem(toDisplay(mod.author)));
    repoTable_->setItem(row, 3, new QTableWidgetItem(mod.rating > 0 ? QString::number(mod.rating) : QString("-")));
    repoTable_->setItem(row, 4, new QTableWidgetItem(toDisplay(mod.note)));
    ++row;
  }

  if (gameDirTable_) {
    const int rowCount = gameDirTable_->rowCount();
    for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
      bool visible = true;
      if (auto* item = gameDirTable_->item(rowIndex, 0)) {
        const int modId = item->data(Qt::UserRole).toInt();
        if (modId > 0) {
          const auto it = std::find_if(mods_.begin(), mods_.end(), [modId](const ModRow& row) {
            return row.id == modId;
          });
          if (it != mods_.end()) {
            visible = modMatchesFilter(*it, attribute, filterId, filterValueText);
          }
        }
      }
      gameDirTable_->setRowHidden(rowIndex, !visible);
    }
  }
}

void MainWindow::populateCategoryFilterModel(QStandardItemModel* model, bool updateCache) {
  if (!model || !repo_) {
    return;
  }

  model->clear();
  auto* uncategorizedItem = new QStandardItem(tr("未分类"));
  uncategorizedItem->setData(kUncategorizedCategoryId, Qt::UserRole);
  model->appendRow(uncategorizedItem);

  const auto categories = repo_->listCategories();
  std::vector<CategoryRow> topLevel;
  std::unordered_map<int, std::vector<CategoryRow>> children;

  for (const auto& category : categories) {
    if (updateCache) {
      categoryNames_[category.id] = QString::fromStdString(category.name);
      if (category.parent_id.has_value()) {
        categoryParent_[category.id] = *category.parent_id;
      }
    }

    if (category.parent_id.has_value()) {
      children[*category.parent_id].push_back(category);
    } else {
      topLevel.push_back(category);
    }
  }

  const auto compare = [](const CategoryRow& a, const CategoryRow& b) {
    if (a.priority != b.priority) return a.priority < b.priority;
    if (a.name != b.name) return a.name < b.name;
    return a.id < b.id;
  };
  std::sort(topLevel.begin(), topLevel.end(), compare);
  for (auto& entry : children) {
    auto& bucket = entry.second;
    std::sort(bucket.begin(), bucket.end(), compare);
  }

  for (const auto& parent : topLevel) {
    auto* parentItem = new QStandardItem(QString::fromStdString(parent.name));
    parentItem->setData(parent.id, Qt::UserRole);
    model->appendRow(parentItem);

    const auto childIt = children.find(parent.id);
    if (childIt != children.end()) {
      for (const auto& child : childIt->second) {
        auto* childItem = new QStandardItem("  " + QString::fromStdString(child.name));
        childItem->setData(child.id, Qt::UserRole);
        model->appendRow(childItem);
      }
    }
  }
}

void MainWindow::populateTagFilterModel(QStandardItemModel* model) const {
  if (!model || !repo_) {
    return;
  }

  model->clear();
  auto* untaggedItem = new QStandardItem(tr("未分类"));
  untaggedItem->setData(kUntaggedTagId, Qt::UserRole);
  model->appendRow(untaggedItem);

  const auto tags = repo_->listTags();
  struct GroupBucket {
    int id = 0;
    QString name;
    int priority = 0;
    std::vector<TagWithGroupRow> tags;
  };

  std::unordered_map<int, GroupBucket> groupedTags;
  for (const auto& tag : tags) {
    auto& bucket = groupedTags[tag.group_id];
    if (bucket.tags.empty()) {
      bucket.id = tag.group_id;
      bucket.name = QString::fromStdString(tag.group_name);
      bucket.priority = tag.group_priority;
    }
    bucket.tags.push_back(tag);
  }

  auto compareGroup = [](const GroupBucket& a, const GroupBucket& b) {
    if (a.priority != b.priority) return a.priority < b.priority;
    if (a.name != b.name) return a.name < b.name;
    return a.id < b.id;
  };
  auto compareTag = [](const TagWithGroupRow& a, const TagWithGroupRow& b) {
    if (a.priority != b.priority) return a.priority < b.priority;
    if (a.name != b.name) return a.name < b.name;
    return a.id < b.id;
  };

  std::vector<GroupBucket> orderedGroups;
  orderedGroups.reserve(groupedTags.size());
  for (auto& entry : groupedTags) {
    auto& bucket = entry.second;
    std::sort(bucket.tags.begin(), bucket.tags.end(), compareTag);
    orderedGroups.push_back(std::move(bucket));
  }
  std::sort(orderedGroups.begin(), orderedGroups.end(), compareGroup);

  for (const auto& group : orderedGroups) {
    auto* groupItem = new QStandardItem(group.name);
    groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsSelectable);
    model->appendRow(groupItem);

    for (const auto& tag : group.tags) {
      auto* tagItem = new QStandardItem("  " + QString::fromStdString(tag.name));
      tagItem->setData(tag.id, Qt::UserRole);
      model->appendRow(tagItem);
    }
  }
}

void MainWindow::populateAuthorFilterModel(QStandardItemModel* model) const {
  if (!model) {
    return;
  }

  model->clear();
  QSet<QString> authors;
  for (const auto& mod : mods_) {
    authors.insert(QString::fromStdString(mod.author));
  }

  QStringList authorList(authors.begin(), authors.end());
  authorList.sort(Qt::CaseInsensitive);
  for (const auto& author : authorList) {
    auto* authorItem = new QStandardItem(author);
    authorItem->setData(author, Qt::UserRole);
    model->appendRow(authorItem);
  }
}

void MainWindow::populateRatingFilterModel(QStandardItemModel* model) const {
  if (!model) {
    return;
  }

  model->clear();
  for (int i = 5; i >= 1; --i) {
    auto* item = new QStandardItem(tr("%1 星").arg(i));
    item->setData(i, Qt::UserRole);
    model->appendRow(item);
  }

  auto* unratedItem = new QStandardItem(tr("未评分"));
  unratedItem->setData(-1, Qt::UserRole);
  model->appendRow(unratedItem);
}

void MainWindow::updateDetailForMod(int modId) {
  const auto it = std::find_if(mods_.begin(), mods_.end(), [modId](const ModRow& row) { return row.id == modId; });
  if (it == mods_.end()) {
    coverLabel_->setPixmap(QPixmap());
    coverLabel_->setText(tr("Selected MOD image"));
    metaLabel_->clear();
    noteView_->clear();
    return;
  }

  const ModRow& mod = *it;
  const QString categoryName = categoryNameFor(mod.category_id);
  QString tags;
  const auto cacheIt = modTagsCache_.find(mod.id);
  if (cacheIt != modTagsCache_.end()) {
    tags = formatTagSummary(cacheIt->second, QStringLiteral("\n"), QStringLiteral(" / "));
  } else {
    auto rows = repo_->listTagsForMod(mod.id);
    tags = formatTagSummary(rows, QStringLiteral("\n"), QStringLiteral(" / "));
  }

  coverLabel_->setText(QString());
  QPixmap pix;
  auto tryLoad = [&](const QString& path) -> bool {
    if (path.isEmpty()) return false;
    QFileInfo info(path);
    if (!info.exists() && !repoDir_.isEmpty()) {
      QDir dir(repoDir_);
      info = QFileInfo(dir.absoluteFilePath(path));
    }
    if (info.exists() && info.isFile()) {
      pix = QPixmap(info.absoluteFilePath());
      if (!pix.isNull()) {
        coverLabel_->setPixmap(pix.scaled(coverLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return true;
      }
    }
    return false;
  };
  if (!tryLoad(QString::fromStdString(mod.cover_path))) {
    coverLabel_->setPixmap(QPixmap());
    coverLabel_->setText(tr("无封面"));
  }

  noteView_->setPlainText(QString::fromStdString(mod.note));

  QStringList meta;
  meta << tr("名称：%1").arg(QString::fromStdString(mod.name));
  meta << tr("分类：%1").arg(categoryName.isEmpty() ? tr("未分类") : categoryName);
  meta << tr("标签：%1").arg(tags.isEmpty() ? tr("无") : tags);
  meta << tr("作者：%1").arg(toDisplay(mod.author, tr("未知")));
  meta << tr("评分：%1").arg(mod.rating > 0 ? QString::number(mod.rating) : tr("未评分"));
  meta << tr("大小：%1 MB").arg(QString::number(mod.size_mb, 'f', 2));
  meta << tr("状态：%1").arg(toDisplay(mod.status, tr("最新")));
  meta << tr("最后发布日：%1").arg(toDisplay(mod.last_published_at, tr("-")));
  meta << tr("最后保存日：%1").arg(toDisplay(mod.last_saved_at, tr("-")));
  meta << tr("健全度：%1").arg(toDisplay(mod.integrity, tr("-")));
  meta << tr("稳定性：%1").arg(toDisplay(mod.stability, tr("-")));
  meta << tr("获取方式：%1").arg(toDisplay(mod.acquisition_method, tr("-")));
  if (!mod.source_platform.empty()) meta << tr("平台：%1").arg(QString::fromStdString(mod.source_platform));
  if (!mod.source_url.empty()) meta << tr("链接：%1").arg(QString::fromStdString(mod.source_url));
  if (!mod.file_path.empty()) meta << tr("文件：%1").arg(QString::fromStdString(mod.file_path));
  if (!mod.file_hash.empty()) meta << tr("哈希：%1").arg(QString::fromStdString(mod.file_hash));
  metaLabel_->setText(meta.join('\n'));
}

QString MainWindow::categoryNameFor(int categoryId) const {
  if (categoryId > 0) {
    const auto it = categoryNames_.find(categoryId);
    if (it != categoryNames_.end()) {
      return it->second;
    }
    return QStringLiteral("Category#%1").arg(categoryId);
  }
  return tr("未分类");
}

bool MainWindow::categoryMatchesFilter(int modCategoryId, int filterCategoryId) const {
  if (filterCategoryId == -1) {
    return modCategoryId == 0;
  }
  if (filterCategoryId <= 0) {
    return true;
  }

  int currentId = modCategoryId;
  while (currentId > 0) {
    if (currentId == filterCategoryId) {
      return true;
    }
    const auto it = categoryParent_.find(currentId);
    if (it == categoryParent_.end()) {
      break;
    }
    currentId = it->second;
  }
  return false;
}

QString MainWindow::tagsTextForMod(int modId) {
  return modTagsText_[modId];
}

QString MainWindow::formatTagSummary(const std::vector<TagWithGroupRow>& rows,
                                     const QString& groupSeparator,
                                     const QString& tagSeparator) const {
  if (rows.empty()) {
    return {};
  }

  QMap<QString, QStringList> grouped;
  for (const auto& row : rows) {
    const QString groupName = QString::fromStdString(row.group_name);
    const QString tagName = QString::fromStdString(row.name);
    QStringList& list = grouped[groupName];
    if (!list.contains(tagName)) {
      list.append(tagName);
    }
  }

  QStringList sections;
  for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
    QStringList tags = it.value();
    std::sort(tags.begin(), tags.end(), [](const QString& a, const QString& b) {
      return a.localeAwareCompare(b) < 0;
    });
    sections << QString("%1: %2").arg(it.key(), tags.join(tagSeparator));
  }
  return sections.join(groupSeparator);
}

std::vector<TagDescriptor> MainWindow::tagsForMod(int modId) const {
  std::vector<TagDescriptor> tags;
  const auto rows = repo_->listTagsForMod(modId);
  tags.reserve(rows.size());
  for (const auto& row : rows) {
    tags.push_back({row.group_name, row.name});
  }
  return tags;
}

void MainWindow::onRefresh() {
  reloadCategories();
  loadData();
}

void MainWindow::onImport() {
  ModEditorDialog dialog(*repo_, this);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }
  ModRow mod = dialog.modData();
  try {
    repo_->createModWithTags(mod, dialog.selectedTags());
    loadData();
  } catch (const DbError& e) {
    QMessageBox::warning(this, tr("导入失败"), tr("MOD 导入失败：%1").arg(e.what()));
  }
}

void MainWindow::onEdit() {
  auto* item = modTable_->currentItem();
  if (!item) {
    QMessageBox::information(this, tr("未选择"), tr("请先选择一个 MOD。"));
    return;
  }
  const int modId = modTable_->item(modTable_->currentRow(), 0)->data(Qt::UserRole).toInt();
  auto mod = repo_->findMod(modId);
  if (!mod) {
    QMessageBox::warning(this, tr("缺失"), tr("该 MOD 记录已不存在。"));
    return;
  }

  ModEditorDialog dialog(*repo_, this);
  dialog.setMod(*mod, tagsForMod(modId));
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  ModRow updated = dialog.modData();
  repo_->updateModWithTags(updated, dialog.selectedTags());
  loadData();
  updateDetailForMod(updated.id);
}

void MainWindow::onApplyToGame() {
  // TODO: implement
}

void MainWindow::onConfigureStrategy() {
  // TODO: implement
}

void MainWindow::onRandomize() {
  // TODO: implement
}

void MainWindow::onSaveCombination() {
  // TODO: implement
}

void MainWindow::onDelete() {
  auto* item = modTable_->currentItem();
  if (!item) return;
  const int modId = modTable_->item(modTable_->currentRow(), 0)->data(Qt::UserRole).toInt();

  const auto reply =
      QMessageBox::question(this, tr("隐藏 MOD"), tr("是否从仓库中隐藏该 MOD？"), QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes) return;

  repo_->setModDeleted(modId, true);
  loadData();
}

void MainWindow::onShowDeletedModsToggled(bool checked) {
  Q_UNUSED(checked);
  populateTable(); // Now just re-filter the already loaded data
}

void MainWindow::onFilterAttributeChanged(const QString& attribute) {
  filterValue_->blockSignals(true);
  filterModel_->clear(); // Clear the model first
  proxyModel_->setFilterFixedString(""); // Clear any active filter on the proxy model

  QString placeholder = "";

  if (attribute == tr("名称")) {
    filterValue_->setEnabled(true);
    placeholder = tr("搜索名称");
  } else if (attribute == tr("分类")) {
    filterValue_->setEnabled(true);
    reloadCategories(); // Populates filterModel_
    placeholder = tr("选择分类");
  } else if (attribute == tr("标签")) {
    filterValue_->setEnabled(true);
    reloadTags(); // Populates filterModel_
    placeholder = tr("选择标签");
  } else if (attribute == tr("作者")) {
    filterValue_->setEnabled(true);
    reloadAuthors(); // Populates filterModel_
    placeholder = tr("搜索作者");
  } else if (attribute == tr("评分")) {
    filterValue_->setEnabled(true);
    reloadRatings(); // Populates filterModel_
    placeholder = tr("选择评分");
  }

  filterValue_->lineEdit()->setPlaceholderText(placeholder); // Set placeholder text

  // Explicitly reset the model to force a refresh
  filterValue_->setModel(nullptr); // Temporarily unset the model
  filterValue_->setModel(proxyModel_); // Set it back
  filterValue_->setEditText(QString()); // 清空输入框以展示占位提示
  filterValue_->setCurrentIndex(-1); // 重置选中项

  filterValue_->blockSignals(false); // Unblock signals here

  onFilterChanged(); // Trigger filter update
}

void MainWindow::reloadRatings() {
  populateRatingFilterModel(filterModel_);
}

void MainWindow::reloadAuthors() {
  populateAuthorFilterModel(filterModel_);
}

void MainWindow::reloadTags() {
  populateTagFilterModel(filterModel_);
}

void MainWindow::onFilterValueTextChanged(const QString& text) {
  proxyModel_->setFilterFixedString(text);

  const QString currentFilter = filterAttribute_->currentText();
  if (currentFilter == tr("分类") || currentFilter == tr("标签")) {
    if (filterValue_->lineEdit()->hasFocus()) {
      filterValue_->showPopup();
    }
  }
}

void MainWindow::onFilterChanged() {
  populateTable();
}

void MainWindow::onSelectorFilterAttributeChanged(const QString& attribute) {
  if (!selectorFilterValue_ || !selectorFilterModel_ || !selectorProxyModel_) {
    return;
  }

  selectorFilterValue_->blockSignals(true);
  selectorFilterModel_->clear();
  selectorProxyModel_->setFilterFixedString("");

  QString placeholder;

  if (attribute == tr("名称")) {
    placeholder = tr("搜索名称");
  } else if (attribute == tr("分类")) {
    populateCategoryFilterModel(selectorFilterModel_, false);
    placeholder = tr("选择分类");
  } else if (attribute == tr("标签")) {
    populateTagFilterModel(selectorFilterModel_);
    placeholder = tr("选择标签");
  } else if (attribute == tr("作者")) {
    populateAuthorFilterModel(selectorFilterModel_);
    placeholder = tr("搜索作者");
  } else if (attribute == tr("评分")) {
    populateRatingFilterModel(selectorFilterModel_);
    placeholder = tr("选择评分");
  }

  if (selectorFilterValue_->lineEdit()) {
    selectorFilterValue_->lineEdit()->setPlaceholderText(placeholder);
  }

  selectorFilterValue_->setModel(nullptr);
  selectorFilterValue_->setModel(selectorProxyModel_);
  selectorFilterValue_->setEditText(QString());
  selectorFilterValue_->setCurrentIndex(-1);

  selectorFilterValue_->blockSignals(false);

  applySelectorFilter();
}

void MainWindow::onSelectorFilterValueTextChanged(const QString& text) {
  if (!selectorProxyModel_) {
    return;
  }
  selectorProxyModel_->setFilterFixedString(text);

  if (!selectorFilterAttribute_ || !selectorFilterValue_ || !selectorFilterValue_->lineEdit()) {
    return;
  }

  const QString currentFilter = selectorFilterAttribute_->currentText();
  if (currentFilter == tr("分类") || currentFilter == tr("标签")) {
    if (selectorFilterValue_->lineEdit()->hasFocus()) {
      selectorFilterValue_->showPopup();
    }
  }
}

void MainWindow::onSelectorFilterChanged() {
  applySelectorFilter();
}

void MainWindow::onCurrentRowChanged(int currentRow, int currentColumn, int previousRow, int previousColumn) {
  Q_UNUSED(currentColumn);
  Q_UNUSED(previousRow);
  Q_UNUSED(previousColumn);
  if (currentRow < 0) {
    updateDetailForMod(-1);
    return;
  }
  auto* item = modTable_->item(currentRow, 0);
  if (!item) {
    updateDetailForMod(-1);
    return;
  }
  updateDetailForMod(item->data(Qt::UserRole).toInt());
}

void MainWindow::switchToRepository() {
  stack_->setCurrentIndex(0);
  updateTabButtonState(repoButton_);
}

void MainWindow::switchToSelector() {
  stack_->setCurrentIndex(1);
  updateTabButtonState(selectorButton_);
  applySelectorFilter();
}

void MainWindow::reloadRepoSelectorData() {
  // 根据当前过滤条件刷新选择器页面展示
  if (selectorFilterAttribute_) {
    onSelectorFilterAttributeChanged(selectorFilterAttribute_->currentText());
  } else {
    applySelectorFilter();
  }
}

void MainWindow::switchToSettings() {
  refreshBasicSettingsUi();
  refreshCategoryManagementUi();
  refreshTagManagementUi();
  refreshDeletionSettingsUi();
  ensureSettingsNavSelection();
  setSettingsStatus({});
  stack_->setCurrentIndex(2);
  updateTabButtonState(settingsButton_);
}

void MainWindow::updateTabButtonState(QPushButton* active) {
  const QList<QPushButton*> buttons = {repoButton_, selectorButton_, settingsButton_};
  for (auto* button : buttons) {
    if (!button) continue;
    button->setCheckable(true);
    button->setChecked(button == active);
    button->setStyleSheet(button == active ? "QPushButton { background: #0f4a70; color: white; }"
                                           : "QPushButton { background: #d0e3ec; }");
  }
}

void MainWindow::onSettingsNavChanged(int row) {
  if (!settingsStack_) {
    return;
  }
  if (row >= 0 && row < settingsStack_->count()) {
    settingsStack_->setCurrentIndex(row);
  }
  setSettingsStatus({});
}

void MainWindow::onBrowseRepoDir() {
  if (!settingsRepoDirEdit_) {
    return;
  }
  const QString current = settingsRepoDirEdit_->text();
  QString selected = QFileDialog::getExistingDirectory(this, tr("选择仓库目录"), current.isEmpty() ? repoDir_ : current);
  if (!selected.isEmpty()) {
    settingsRepoDirEdit_->setText(QDir::cleanPath(selected));
  }
}

void MainWindow::onBrowseGameDir() {
  if (!settingsGameDirEdit_) {
    return;
  }
  const QString current = settingsGameDirEdit_->text();
  QString selected = QFileDialog::getExistingDirectory(this, tr("选择游戏目录"), current);
  if (!selected.isEmpty()) {
    settingsGameDirEdit_->setText(QDir::cleanPath(selected));
  }
}

void MainWindow::onImportModeChanged(int /*index*/) {
  setSettingsStatus({});
}

void MainWindow::onAutoImportToggled(bool checked) {
  if (autoImportModeCombo_) {
    autoImportModeCombo_->setEnabled(checked);
  }
  setSettingsStatus({});
}

void MainWindow::onAutoImportModeChanged(int /*index*/) {
  setSettingsStatus({});
}

void MainWindow::onSaveSettings() {
  try {
    Settings updated = settings_;
    const QString appDirPath = QCoreApplication::applicationDirPath();
    const QString databaseDirPath = QDir(appDirPath).filePath("database");

    if (settingsRepoDirEdit_) {
      const QString repoPath = QDir::cleanPath(QDir::fromNativeSeparators(settingsRepoDirEdit_->text().trimmed()));
      if (repoPath.isEmpty()) {
        setSettingsStatus(tr("仓库目录不能为空"), true);
        return;
      }
      QDir repoDir(repoPath);
      if (!repoDir.exists()) {
        if (!repoDir.mkpath(".")) {
          setSettingsStatus(tr("无法创建仓库目录"), true);
          return;
        }
      }
      updated.repoDir = repoPath.toStdString();
      updated.repoDbPath = QDir(repoPath).filePath("repo.db").toStdString();
    }

    if (settingsGameDirEdit_) {
      const QString gamePath = QDir::cleanPath(QDir::fromNativeSeparators(settingsGameDirEdit_->text().trimmed()));
      updated.gameDirectory = gamePath.toStdString();
    }

    if (importModeCombo_) {
      updated.importAction = static_cast<ImportAction>(importModeCombo_->currentData().toInt());
    }

    if (autoImportCheckbox_) {
      updated.addonsAutoImportEnabled = autoImportCheckbox_->isChecked();
    }

    if (autoImportModeCombo_) {
      updated.addonsAutoImportMethod = static_cast<AddonsAutoImportMethod>(autoImportModeCombo_->currentData().toInt());
    }

    if (retainDeletedCheckbox_) {
      updated.retainDataOnDelete = retainDeletedCheckbox_->isChecked();
    }

    QDir dbDir(databaseDirPath);
    if (!dbDir.exists() && !dbDir.mkpath(".")) {
      setSettingsStatus(tr("无法创建数据库目录"), true);
      return;
    }
    updated.repoDbPath = QDir(appDirPath).filePath("database/repo.db").toStdString();

    updated.save();

    const bool repoChanged = updated.repoDir != settings_.repoDir;
    settings_ = updated;

    if (repoChanged) {
      reinitializeRepository(settings_);
    } else {
      reloadCategories();
      loadData();
      reloadRepoSelectorData();
    }

    refreshBasicSettingsUi();
    refreshCategoryManagementUi();
    refreshTagManagementUi();
    refreshDeletionSettingsUi();
    setSettingsStatus(tr("设置已保存"));
  } catch (const std::exception& ex) {
    setSettingsStatus(QString::fromUtf8(ex.what()), true);
  }
}

void MainWindow::onCategorySelectionChanged() {
  const bool hasSelection = selectedCategoryId() > 0;
  if (categoryAddChildBtn_) categoryAddChildBtn_->setEnabled(hasSelection);
  if (categoryRenameBtn_) categoryRenameBtn_->setEnabled(hasSelection);
  if (categoryDeleteBtn_) categoryDeleteBtn_->setEnabled(hasSelection);

  bool canMoveUp = false;
  bool canMoveDown = false;
  if (hasSelection && categoryTree_) {
    if (auto* item = categoryTree_->currentItem()) {
      QTreeWidgetItem* parent = item->parent();
      int index = parent ? parent->indexOfChild(item) : categoryTree_->indexOfTopLevelItem(item);
      int siblingCount = parent ? parent->childCount() : categoryTree_->topLevelItemCount();
      if (index >= 0) {
        canMoveUp = index > 0;
        canMoveDown = index + 1 < siblingCount;
      }
    }
  }
  if (categoryMoveUpBtn_) categoryMoveUpBtn_->setEnabled(canMoveUp);
  if (categoryMoveDownBtn_) categoryMoveDownBtn_->setEnabled(canMoveDown);
}

void MainWindow::onCategoryItemChanged(QTreeWidgetItem* item, int column) {
  if (suppressCategoryItemSignals_ || !repo_ || !item) {
    return;
  }
  const int id = item->data(0, Qt::UserRole).toInt();
  if (id <= 0) {
    return;
  }

  std::optional<int> parentId;
  const QVariant parentData = item->data(0, Qt::UserRole + 1);
  if (parentData.isValid()) {
    parentId = parentData.toInt();
  }
  const QString originalName = item->data(0, Qt::UserRole + 2).toString();
  const int originalPriority = item->data(1, Qt::UserRole).toInt();

  auto restoreName = [&]() {
    suppressCategoryItemSignals_ = true;
    item->setText(0, originalName);
    suppressCategoryItemSignals_ = false;
  };
  auto restorePriority = [&]() {
    suppressCategoryItemSignals_ = true;
    item->setText(1, QString::number(originalPriority));
    suppressCategoryItemSignals_ = false;
  };

  if (column == 0) {
    const QString newName = item->text(0).trimmed();
    if (newName.isEmpty()) {
      restoreName();
      return;
    }
    if (newName == originalName) {
      item->setText(0, originalName);
      return;
    }
    try {
      repo_->updateCategory(id, newName.toStdString(), parentId, std::nullopt);
      refreshCategoryManagementUi();
      reloadCategories();
      loadData();
    } catch (const std::exception& ex) {
      restoreName();
      QMessageBox::warning(this, tr("更新失败"), QString::fromUtf8(ex.what()));
    }
  } else if (column == 1) {
    bool ok = false;
    const QString textValue = item->text(1).trimmed();
    const int newPriority = textValue.toInt(&ok);
    if (!ok || newPriority <= 0) {
      restorePriority();
      return;
    }
    if (newPriority == originalPriority) {
      item->setText(1, QString::number(originalPriority));
      return;
    }
    try {
      const QString currentName = item->text(0).trimmed();
      repo_->updateCategory(id, currentName.toStdString(), parentId, newPriority);
      refreshCategoryManagementUi();
      reloadCategories();
      loadData();
    } catch (const std::exception& ex) {
      restorePriority();
      QMessageBox::warning(this, tr("更新失败"), QString::fromUtf8(ex.what()));
    }
  }
}

void MainWindow::adjustCategoryOrder(int direction) {
  if (!repo_ || !categoryTree_ || direction == 0) {
    return;
  }
  auto* item = categoryTree_->currentItem();
  if (!item) {
    return;
  }

  QTreeWidgetItem* parent = item->parent();
  const int index = parent ? parent->indexOfChild(item) : categoryTree_->indexOfTopLevelItem(item);
  const int siblingCount = parent ? parent->childCount() : categoryTree_->topLevelItemCount();
  const int targetIndex = index + direction;
  if (index < 0 || targetIndex < 0 || targetIndex >= siblingCount) {
    return;
  }

  QTreeWidgetItem* sibling = parent ? parent->child(targetIndex) : categoryTree_->topLevelItem(targetIndex);
  if (!sibling) {
    return;
  }

  const int currentId = item->data(0, Qt::UserRole).toInt();
  const int siblingId = sibling->data(0, Qt::UserRole).toInt();
  if (currentId <= 0 || siblingId <= 0) {
    return;
  }

  try {
    repo_->swapCategoryPriority(currentId, siblingId);
    refreshCategoryManagementUi();
    reloadCategories();
    loadData();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("更新失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onMoveCategoryUp() {
  adjustCategoryOrder(-1);
}

void MainWindow::onMoveCategoryDown() {
  adjustCategoryOrder(1);
}

void MainWindow::onAddCategoryTopLevel() {
  if (!repo_) {
    return;
  }
  bool ok = false;
  const QString name = QInputDialog::getText(this, tr("新增分类"), tr("分类名称"), QLineEdit::Normal, {}, &ok).trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  try {
    repo_->createCategory(name.toStdString(), std::nullopt);
    refreshCategoryManagementUi();
    reloadCategories();
    loadData();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("创建失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onAddCategoryChild() {
  if (!repo_) {
    return;
  }
  const int parentId = selectedCategoryId();
  if (parentId <= 0) {
    QMessageBox::information(this, tr("提示"), tr("请先选择一个父级分类"));
    return;
  }
  bool ok = false;
  const QString name = QInputDialog::getText(this, tr("新增子分类"), tr("分类名称"), QLineEdit::Normal, {}, &ok).trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  try {
    repo_->createCategory(name.toStdString(), parentId);
    refreshCategoryManagementUi();
    reloadCategories();
    loadData();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("创建失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onRenameCategory() {
  if (!repo_ || !categoryTree_) {
    return;
  }
  auto* item = categoryTree_->currentItem();
  if (!item) {
    return;
  }
  const int id = item->data(0, Qt::UserRole).toInt();
  if (id <= 0) {
    return;
  }
  bool ok = false;
  const QString currentName = item->text(0);
  const QString name = QInputDialog::getText(this, tr("重命名分类"), tr("分类名称"), QLineEdit::Normal, currentName, &ok).trimmed();
  if (!ok || name.isEmpty() || name == currentName) {
    return;
  }
  std::optional<int> parentId;
  const QVariant parentData = item->data(0, Qt::UserRole + 1);
  if (parentData.isValid()) {
    parentId = parentData.toInt();
  }
  try {
    repo_->updateCategory(id, name.toStdString(), parentId);
    refreshCategoryManagementUi();
    reloadCategories();
    loadData();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("重命名失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onDeleteCategory() {
  if (!repo_) {
    return;
  }
  const int id = selectedCategoryId();
  if (id <= 0) {
    return;
  }
  QTreeWidgetItem* currentItem = categoryTree_ ? categoryTree_->currentItem() : nullptr;
  const bool hasChildren = currentItem && currentItem->childCount() > 0;
  const bool isTopLevel = currentItem && !currentItem->data(0, Qt::UserRole + 1).isValid();

  const QString prompt = hasChildren
                              ? tr("删除该分类将同时删除其所有子分类，并清空相关 MOD 分类信息。是否继续？")
                              : tr("删除该分类将清空相关 MOD 分类信息。是否继续？");
  if (QMessageBox::question(this, tr("删除分类"), prompt) != QMessageBox::Yes) {
    return;
  }

  if (isTopLevel && hasChildren) {
    const QString confirmation = tr("这是一级分类，删除后会清空全部子分类及其分类信息。请再次确认是否执行删除。");
    if (QMessageBox::question(this, tr("再次确认"), confirmation) != QMessageBox::Yes) {
      return;
    }
  }

  try {
    repo_->deleteCategory(id);
    refreshCategoryManagementUi();
    reloadCategories();
    loadData();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("删除失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onTagGroupSelectionChanged(int row) {
  Q_UNUSED(row);
  refreshTagListForGroup(selectedTagGroupId());
}

void MainWindow::onAddTagGroup() {
  if (!repo_) {
    return;
  }
  bool ok = false;
  const QString name = QInputDialog::getText(this, tr("新增标签组"), tr("组名称"), QLineEdit::Normal, {}, &ok).trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  try {
    repo_->createTagGroup(name.toStdString());
    refreshTagManagementUi();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("创建失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onRenameTagGroup() {
  if (!repo_ || !tagGroupList_) {
    return;
  }
  auto* item = tagGroupList_->currentItem();
  if (!item) {
    return;
  }
  const int groupId = item->data(Qt::UserRole).toInt();
  if (groupId <= 0) {
    return;
  }
  bool ok = false;
  const QString currentName = item->text();
  const QString name = QInputDialog::getText(this, tr("重命名标签组"), tr("组名称"), QLineEdit::Normal, currentName, &ok).trimmed();
  if (!ok || name.isEmpty() || name == currentName) {
    return;
  }
  try {
    repo_->renameTagGroup(groupId, name.toStdString());
    refreshTagManagementUi();
    loadData();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("重命名失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onDeleteTagGroup() {
  if (!repo_) {
    return;
  }
  const int groupId = selectedTagGroupId();
  if (groupId <= 0) {
    return;
  }
  const auto reply = QMessageBox::question(this, tr("删除标签组"), tr("仅当该组没有标签时才能删除，是否继续？"));
  if (reply != QMessageBox::Yes) {
    return;
  }
  try {
    if (!repo_->deleteTagGroup(groupId)) {
      QMessageBox::information(this, tr("无法删除"), tr("该组仍包含标签，请先删除所有标签。"));
      return;
    }
    refreshTagManagementUi();
    loadData();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("删除失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onAddTag() {
  if (!repo_) {
    return;
  }
  const int groupId = selectedTagGroupId();
  if (groupId <= 0) {
    QMessageBox::information(this, tr("提示"), tr("请先选择标签组"));
    return;
  }
  bool ok = false;
  const QString name = QInputDialog::getText(this, tr("新增标签"), tr("标签名称"), QLineEdit::Normal, {}, &ok).trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  try {
    repo_->createTag(groupId, name.toStdString());
    refreshTagListForGroup(groupId);
    loadData();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("创建失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onRenameTag() {
  if (!repo_ || !tagList_) {
    return;
  }
  auto* item = tagList_->currentItem();
  if (!item) {
    return;
  }
  const int tagId = item->data(Qt::UserRole).toInt();
  if (tagId <= 0) {
    return;
  }
  bool ok = false;
  const QString currentName = item->text();
  const QString name = QInputDialog::getText(this, tr("重命名标签"), tr("标签名称"), QLineEdit::Normal, currentName, &ok).trimmed();
  if (!ok || name.isEmpty() || name == currentName) {
    return;
  }
  try {
    repo_->renameTag(tagId, name.toStdString());
    refreshTagListForGroup(selectedTagGroupId());
    loadData();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("重命名失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onDeleteTag() {
  if (!repo_) {
    return;
  }
  const int tagId = selectedTagId();
  if (tagId <= 0) {
    return;
  }
  const auto reply = QMessageBox::question(this, tr("删除标签"), tr("仅当标签未被 MOD 使用时才能删除，是否继续？"));
  if (reply != QMessageBox::Yes) {
    return;
  }
  try {
    if (!repo_->deleteTag(tagId)) {
      QMessageBox::information(this, tr("无法删除"), tr("仍有 MOD 使用该标签，请先解除绑定。"));
      return;
    }
    refreshTagListForGroup(selectedTagGroupId());
    loadData();
  } catch (const std::exception& ex) {
    QMessageBox::warning(this, tr("删除失败"), QString::fromUtf8(ex.what()));
  }
}

void MainWindow::onTagSelectionChanged(int row) {
  const bool hasSelection = row >= 0 && selectedTagId() > 0;
  if (tagRenameBtn_) tagRenameBtn_->setEnabled(hasSelection);
  if (tagDeleteBtn_) tagDeleteBtn_->setEnabled(hasSelection);
}

void MainWindow::onClearDeletedMods() {
  const auto reply =
      QMessageBox::question(this, tr("清除已删除MOD数据"),
                            tr("这将永久删除所有已标记为删除的MOD数据记录。此操作不可撤销。确定要继续吗？"),
                            QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }

  try {
    repo_->clearDeletedMods(); // This method needs to be implemented in RepositoryService
    loadData(); // Refresh the table
    QMessageBox::information(this, tr("清除成功"), tr("已成功清除所有已删除MOD的数据记录。"));
  } catch (const DbError& e) {
    QMessageBox::warning(this, tr("清除失败"), tr("清除已删除MOD数据记录失败：%1").arg(e.what()));
  }
}

QString MainWindow::detectL4D2GameDirectory() const {
  QString installPath;
  // Try 64-bit registry path first
  QSettings settings64("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 550", QSettings::NativeFormat);
  installPath = settings64.value("InstallLocation").toString();

  if (installPath.isEmpty()) {
    // Try 32-bit registry path (for 32-bit apps on 64-bit Windows)
    QSettings settings32("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 550", QSettings::NativeFormat);
    installPath = settings32.value("InstallLocation").toString();
  }

  if (!installPath.isEmpty()) {
    // Verify if it's actually L4D2 by checking for a known file/folder
    QDir gameDir(installPath);
    if (gameDir.exists("left4dead2")) { // Check for the 'left4dead2' folder inside the install path
      return QDir::toNativeSeparators(installPath);
    }
  }
  return QString(); // Return empty if not found or not verified
}
