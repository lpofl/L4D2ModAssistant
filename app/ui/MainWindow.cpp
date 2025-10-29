#include "app/ui/MainWindow.h"

#include <algorithm>
#include <functional>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication> // For applicationDirPath()
#include <QDir>             // For mkpath()
#include <QFile>            // For copy/move operations
#include <QSettings>        // For registry access
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QStandardItem>
#include <QSet>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include "app/ui/ModEditorDialog.h"
#include "app/ui/components/ModFilterPanel.h"
#include "app/ui/components/ModTableWidget.h"
#include "app/ui/components/NavigationBar.h"
#include "app/ui/pages/RepositoryPage.h"
#include "app/ui/pages/SettingsPage.h"
#include "app/ui/pages/SelectorPage.h"
#include "app/ui/presenters/RepositoryPresenter.h"
#include "app/ui/presenters/SelectorPresenter.h"
#include "app/services/ApplicationInitializer.h" // 应用初始化与装配
#include "core/config/Settings.h"
#include "core/db/Db.h"
#include "core/db/Migrations.h"
#include "core/log/Log.h"

#include <QSortFilterProxyModel>
#include <QStandardItemModel>

namespace {


QString toDisplay(const std::string& value, const QString& fallback = {}) {
  return value.empty() ? fallback : QString::fromStdString(value);
}

} // namespace

MainWindow::~MainWindow() = default;

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
      const QString normalizedRoot = normalizeRootInput(detectedPath);
      settings_.gameDirectory = normalizedRoot.toStdString();
      // 新增：根据游戏目录推导并保存 addons 与 workshop 路径（便于其它场景直接读取）
      const QString addonsPath = deriveAddonsPath(normalizedRoot);
      const QString workshopPath = deriveWorkshopPath(addonsPath);
      settings_.addonsPath = QDir::toNativeSeparators(addonsPath).toStdString();
      settings_.workshopPath = QDir::toNativeSeparators(workshopPath).toStdString();
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

  navigationBar_ = new NavigationBar(central);
  rootLayout->addWidget(navigationBar_);

  connect(navigationBar_, &NavigationBar::repositoryRequested, this, &MainWindow::switchToRepository);
  connect(navigationBar_, &NavigationBar::selectorRequested, this, &MainWindow::switchToSelector);
  connect(navigationBar_, &NavigationBar::settingsRequested, this, &MainWindow::switchToSettings);

  stack_ = new QStackedWidget(central);
  repositoryPage_ = new RepositoryPage(stack_);
  stack_->addWidget(repositoryPage_);
  repoFilterPanel_ = repositoryPage_->filterPanel();
  filterAttribute_ = repoFilterPanel_ ? repoFilterPanel_->attributeCombo() : nullptr;
  filterValue_ = repoFilterPanel_ ? repoFilterPanel_->valueCombo() : nullptr;
  showDeletedModsCheckBox_ = repositoryPage_->showDeletedCheckBox();
  modTable_ = repositoryPage_->modTable();
  importBtn_ = repositoryPage_->importButton();
  editBtn_ = repositoryPage_->editButton();
  deleteBtn_ = repositoryPage_->deleteButton();
  refreshBtn_ = repositoryPage_->refreshButton();
  coverLabel_ = repositoryPage_->coverLabel();
  metaLabel_ = repositoryPage_->metaLabel();
  noteView_ = repositoryPage_->noteView();

  // 选择器页装配，界面与业务逻辑转交给 SelectorPage/SelectorPresenter
  selectorPage_ = new SelectorPage(stack_);
  stack_->addWidget(selectorPage_);
  selectorPresenter_ = std::make_unique<SelectorPresenter>(selectorPage_, this);
  selectorPresenter_->initializeFilters();

  configureStrategyBtn_ = selectorPage_->configureStrategyButton();
  randomizeBtn_ = selectorPage_->randomizeButton();
  saveCombinationBtn_ = selectorPage_->saveCombinationButton();
  applyToGameBtn_ = selectorPage_->applyButton();
  strategyInfoLabel_ = selectorPage_->strategyInfoLabel();

  connect(selectorPage_, &SelectorPage::configureStrategyRequested, this, &MainWindow::onConfigureStrategy);
  connect(selectorPage_, &SelectorPage::randomizeRequested, this, &MainWindow::onRandomize);
  connect(selectorPage_, &SelectorPage::saveCombinationRequested, this, &MainWindow::onSaveCombination);
  connect(selectorPage_, &SelectorPage::applyRequested, this, &MainWindow::onApplyToGame);

  settingsPage_ = new SettingsPage(stack_);
  stack_->addWidget(settingsPage_);

  // 设置页控件由 SettingsPage 构建，此处仅绑定引用并连接信号，保持 MainWindow 专注业务逻辑
  settingsNav_ = settingsPage_->navigation();
  settingsStack_ = settingsPage_->stack();
  settingsRepoDirEdit_ = settingsPage_->repoDirEdit();
  settingsRepoBrowseBtn_ = settingsPage_->repoBrowseButton();
  settingsGameDirEdit_ = settingsPage_->gameDirEdit();
  settingsGameDirBrowseBtn_ = settingsPage_->gameDirBrowseButton();
  settingsAddonsPathDisplay_ = settingsPage_->addonsDisplay();
  settingsWorkshopPathDisplay_ = settingsPage_->workshopDisplay();
  importModeCombo_ = settingsPage_->importModeCombo();
  autoImportCheckbox_ = settingsPage_->autoImportCheck();
  autoImportModeCombo_ = settingsPage_->autoImportModeCombo();
  saveSettingsBtn_ = settingsPage_->saveSettingsButton();
  settingsStatusLabel_ = settingsPage_->statusLabel();

  categoryTree_ = settingsPage_->categoryTree();
  categoryAddRootBtn_ = settingsPage_->categoryAddRootButton();
  categoryAddChildBtn_ = settingsPage_->categoryAddChildButton();
  categoryRenameBtn_ = settingsPage_->categoryRenameButton();
  categoryDeleteBtn_ = settingsPage_->categoryDeleteButton();
  categoryMoveUpBtn_ = settingsPage_->categoryMoveUpButton();
  categoryMoveDownBtn_ = settingsPage_->categoryMoveDownButton();

  tagGroupList_ = settingsPage_->tagGroupList();
  tagList_ = settingsPage_->tagList();
  tagGroupAddBtn_ = settingsPage_->tagGroupAddButton();
  tagGroupRenameBtn_ = settingsPage_->tagGroupRenameButton();
  tagGroupDeleteBtn_ = settingsPage_->tagGroupDeleteButton();
  tagAddBtn_ = settingsPage_->tagAddButton();
  tagRenameBtn_ = settingsPage_->tagRenameButton();
  tagDeleteBtn_ = settingsPage_->tagDeleteButton();

  retainDeletedCheckbox_ = settingsPage_->retainDeletedCheck();
  clearDeletedModsBtn_ = settingsPage_->clearDeletedButton();

  if (settingsNav_) {
    connect(settingsNav_, &QListWidget::currentRowChanged, this, &MainWindow::onSettingsNavChanged);
  }
  if (settingsRepoBrowseBtn_) {
    connect(settingsRepoBrowseBtn_, &QPushButton::clicked, this, &MainWindow::onBrowseRepoDir);
  }
  if (settingsGameDirBrowseBtn_) {
    connect(settingsGameDirBrowseBtn_, &QPushButton::clicked, this, &MainWindow::onBrowseGameDir);
  }
  if (settingsGameDirEdit_) {
    connect(settingsGameDirEdit_, &QLineEdit::textChanged, this, &MainWindow::onGameDirEdited);
  }
  if (importModeCombo_) {
    connect(importModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onImportModeChanged);
  }
  if (autoImportCheckbox_) {
    connect(autoImportCheckbox_, &QCheckBox::toggled, this, &MainWindow::onAutoImportToggled);
  }
  if (autoImportModeCombo_) {
    connect(autoImportModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onAutoImportModeChanged);
  }
  if (saveSettingsBtn_) {
    connect(saveSettingsBtn_, &QPushButton::clicked, this, &MainWindow::onSaveSettings);
  }
  if (categoryTree_) {
    connect(categoryTree_, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onCategorySelectionChanged);
    connect(categoryTree_, &QTreeWidget::itemChanged, this, &MainWindow::onCategoryItemChanged);
  }
  if (categoryAddRootBtn_) {
    connect(categoryAddRootBtn_, &QPushButton::clicked, this, &MainWindow::onAddCategoryTopLevel);
  }
  if (categoryAddChildBtn_) {
    connect(categoryAddChildBtn_, &QPushButton::clicked, this, &MainWindow::onAddCategoryChild);
  }
  if (categoryRenameBtn_) {
    connect(categoryRenameBtn_, &QPushButton::clicked, this, &MainWindow::onRenameCategory);
  }
  if (categoryDeleteBtn_) {
    connect(categoryDeleteBtn_, &QPushButton::clicked, this, &MainWindow::onDeleteCategory);
  }
  if (categoryMoveUpBtn_) {
    connect(categoryMoveUpBtn_, &QPushButton::clicked, this, &MainWindow::onMoveCategoryUp);
  }
  if (categoryMoveDownBtn_) {
    connect(categoryMoveDownBtn_, &QPushButton::clicked, this, &MainWindow::onMoveCategoryDown);
  }
  if (tagGroupList_) {
    connect(tagGroupList_, &QListWidget::currentRowChanged, this, &MainWindow::onTagGroupSelectionChanged);
  }
  if (tagList_) {
    connect(tagList_, &QListWidget::currentRowChanged, this, &MainWindow::onTagSelectionChanged);
  }
  if (tagGroupAddBtn_) {
    connect(tagGroupAddBtn_, &QPushButton::clicked, this, &MainWindow::onAddTagGroup);
  }
  if (tagGroupRenameBtn_) {
    connect(tagGroupRenameBtn_, &QPushButton::clicked, this, &MainWindow::onRenameTagGroup);
  }
  if (tagGroupDeleteBtn_) {
    connect(tagGroupDeleteBtn_, &QPushButton::clicked, this, &MainWindow::onDeleteTagGroup);
  }
  if (tagAddBtn_) {
    connect(tagAddBtn_, &QPushButton::clicked, this, &MainWindow::onAddTag);
  }
  if (tagRenameBtn_) {
    connect(tagRenameBtn_, &QPushButton::clicked, this, &MainWindow::onRenameTag);
  }
  if (tagDeleteBtn_) {
    connect(tagDeleteBtn_, &QPushButton::clicked, this, &MainWindow::onDeleteTag);
  }
  if (clearDeletedModsBtn_) {
    connect(clearDeletedModsBtn_, &QPushButton::clicked, this, &MainWindow::onClearDeletedMods);
  }

  ensureSettingsNavSelection();
  rootLayout->addWidget(stack_, 1);

  setCentralWidget(central);
  resize(1280, 760);
  setWindowTitle(QStringLiteral("L4D2 MOD 助手"));
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
    const QString rootPath = QString::fromStdString(settings_.gameDirectory);
    settingsGameDirEdit_->setText(QDir::toNativeSeparators(rootPath));
    updateDerivedGamePaths(rootPath);
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
  repo_ = ApplicationInitializer::createRepositoryService(settings);
  if (!importService_) {
    importService_ = std::make_unique<ImportService>();
  }
  if (!randomizeController_ && repo_) {
    randomizeController_ = std::make_unique<RandomizeController>(*repo_);
  }

  if (!repositoryPresenter_) {
    repositoryPresenter_ = std::make_unique<RepositoryPresenter>(repositoryPage_, settings_, this, this);
    connect(repositoryPresenter_.get(), &RepositoryPresenter::modsReloaded, this, &MainWindow::onRepositoryModsReloaded);
    repositoryPresenter_->initializeFilters();
  }

  repositoryPresenter_->setRepositoryService(repo_.get());
  repositoryPresenter_->setImportService(importService_.get());
  repositoryPresenter_->setRepositoryDirectory(repoDir_);
  repositoryPresenter_->reloadAll();

  if (selectorPresenter_) {
    selectorPresenter_->setRepositoryPresenter(repositoryPresenter_.get());
    selectorPresenter_->refreshRepositoryData();
  }
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





void MainWindow::applySelectorFilter() {
  if (selectorPresenter_) {
    selectorPresenter_->applyFilter();
  }
}

void MainWindow::populateCategoryFilterModel(QStandardItemModel* model, bool updateCache) {
  if (repositoryPresenter_) {
    repositoryPresenter_->populateCategoryFilterModel(model, updateCache);
  }
}

void MainWindow::populateTagFilterModel(QStandardItemModel* model) const {
  if (repositoryPresenter_) {
    repositoryPresenter_->populateTagFilterModel(model);
  }
}

void MainWindow::populateAuthorFilterModel(QStandardItemModel* model) const {
  if (repositoryPresenter_) {
    repositoryPresenter_->populateAuthorFilterModel(model);
  }
}

void MainWindow::populateRatingFilterModel(QStandardItemModel* model) const {
  if (repositoryPresenter_) {
    repositoryPresenter_->populateRatingFilterModel(model);
  }
}


void MainWindow::onApplyToGame() {
  // TODO: implement
}

void MainWindow::onConfigureStrategy() {
  // TODO: implement
}

void MainWindow::onRandomize() {
  // 调用控制器执行随机组合，此处仅展示一个最小可用流程，后续可由 Selector 页面接管展示
  if (!randomizeController_) {
    QMessageBox::warning(this, tr("未准备就绪"), tr("随机器尚未初始化"));
    return;
  }
  RandomizerConfig cfg; // 使用默认配置，后续由 SelectorViewModel 提供配置来源
  const auto result = randomizeController_->randomize(cfg);
  QMessageBox::information(this, tr("随机完成"),
                           tr("生成方案数：%1，合计大小：%2 MB")
                               .arg(static_cast<int>(result.entries.size()))
                               .arg(result.total_size_mb, 0, 'f', 1));
}

void MainWindow::onSaveCombination() {
  // TODO: implement
}












void MainWindow::switchToRepository() {
  stack_->setCurrentIndex(0);
  if (navigationBar_) {
    navigationBar_->setActive(NavigationBar::Tab::Repository);
  }
}

void MainWindow::switchToSelector() {
  stack_->setCurrentIndex(1);
  if (navigationBar_) {
    navigationBar_->setActive(NavigationBar::Tab::Selector);
  }
  applySelectorFilter();
}

void MainWindow::reloadRepoSelectorData() {
  if (selectorPresenter_) {
    selectorPresenter_->refreshRepositoryData();
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
  if (navigationBar_) {
    navigationBar_->setActive(NavigationBar::Tab::Settings);
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
    const QString cleanedRoot = normalizeRootInput(selected); // 清理输入确保为根目录格式
    settingsGameDirEdit_->setText(QDir::toNativeSeparators(cleanedRoot));
    updateDerivedGamePaths(cleanedRoot);
  }
}

void MainWindow::onGameDirEdited(const QString& path) {
  const QString cleanedRoot = normalizeRootInput(path); // 文本改变时同步清理
  updateDerivedGamePaths(cleanedRoot);
  setSettingsStatus({});
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
      const QString cleanedRoot = normalizeRootInput(settingsGameDirEdit_->text().trimmed());
      if (cleanedRoot.isEmpty()) {
        setSettingsStatus(tr("游戏根目录不能为空"), true);
        return;
      }
      const QString addonsPath = deriveAddonsPath(cleanedRoot);
      if (addonsPath.isEmpty()) {
        setSettingsStatus(tr("未能识别有效的 addons 目录，请确认选择了正确的游戏根目录"), true);
        return;
      }
      updated.gameDirectory = cleanedRoot.toStdString();
      // 新增：同时保存推导得到的 addons 与 workshop 路径，确保设置可被其它场景直接使用
      {
        const QString normalizedRoot = cleanedRoot;
        const QString addons = deriveAddonsPath(normalizedRoot);
        const QString workshop = deriveWorkshopPath(addons);
        updated.addonsPath = QDir::toNativeSeparators(addons).toStdString();
        updated.workshopPath = QDir::toNativeSeparators(workshop).toStdString();
      }
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
      if (repositoryPresenter_) {
        repositoryPresenter_->reloadAll();
      }
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
      if (repositoryPresenter_) {
        repositoryPresenter_->reloadAll();
      }
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
      if (repositoryPresenter_) {
        repositoryPresenter_->reloadAll();
      }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    }
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
    if (repositoryPresenter_) {
      repositoryPresenter_->reloadAll();
    } // Refresh the table
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

QString MainWindow::deriveAddonsPath(const QString& rootPath) const {
  const QString normalizedRoot = normalizeRootInput(rootPath);
  if (normalizedRoot.isEmpty()) {
    return {};
  }

  QDir rootDir(normalizedRoot);
  const QString dirName = rootDir.dirName().toLower();

  if (dirName == QStringLiteral("addons")) {
    return normalizedRoot;
  }

  if (dirName == QStringLiteral("left4dead2")) {
    return QDir::cleanPath(rootDir.filePath(QStringLiteral("addons")));
  }

  return QDir::cleanPath(rootDir.filePath(QStringLiteral("left4dead2/addons")));
}

QString MainWindow::deriveWorkshopPath(const QString& addonsPath) const {
  if (addonsPath.isEmpty()) {
    return {};
  }
  QDir addonsDir(addonsPath);
  return QDir::cleanPath(addonsDir.filePath(QStringLiteral("workshop")));
}

void MainWindow::updateDerivedGamePaths(const QString& rootPath) {
  if (!settingsAddonsPathDisplay_ || !settingsWorkshopPathDisplay_) {
    return;
  }

  const QString normalizedRoot = normalizeRootInput(rootPath);
  const QString addonsPath = deriveAddonsPath(normalizedRoot);
  const QString workshopPath = deriveWorkshopPath(addonsPath);

  {
    QSignalBlocker blocker(settingsAddonsPathDisplay_);
    settingsAddonsPathDisplay_->setText(
        addonsPath.isEmpty() ? QString() : QDir::toNativeSeparators(addonsPath));
  }
  {
    QSignalBlocker blocker(settingsWorkshopPathDisplay_);
    settingsWorkshopPathDisplay_->setText(
        workshopPath.isEmpty() ? QString() : QDir::toNativeSeparators(workshopPath));
  }
}

QString MainWindow::normalizeRootInput(const QString& rawPath) const {
  if (rawPath.isEmpty()) {
    return {};
  }
  QString cleaned = QDir::cleanPath(QDir::fromNativeSeparators(rawPath));
  QDir dir(cleaned);
  const QString dirName = dir.dirName().toLower();

  if (dirName == QStringLiteral("addons")) {
    if (dir.cdUp()) {
      QString candidate = QDir::cleanPath(dir.path());
      if (dir.dirName().compare(QStringLiteral("left4dead2"), Qt::CaseInsensitive) == 0 && dir.cdUp()) {
        return QDir::cleanPath(dir.path());
      }
      return candidate;
    }
  }

  if (dirName == QStringLiteral("left4dead2")) {
    if (dir.cdUp()) {
      return QDir::cleanPath(dir.path());
    }
  }

  return cleaned;
}

bool MainWindow::ensureModFilesInRepository(ModRow& mod, QStringList& errors) const {
  const ImportAction action = settings_.importAction;
  if (action == ImportAction::None) {
    return true;
  }

  const QString repoDir = QDir::cleanPath(QDir::fromNativeSeparators(QString::fromStdString(settings_.repoDir)));
  if (repoDir.isEmpty()) {
    errors << tr("仓库目录未配置，无法执行入库操作。");
    return false;
  }

  QDir repoDirObj(repoDir);
  if (!repoDirObj.exists() && !repoDirObj.mkpath(QStringLiteral("."))) {
    errors << tr("无法创建仓库目录：%1").arg(repoDir);
    return false;
  }

  const auto repoPrefix = [repoDirObj]() {
    QString prefix = QDir::fromNativeSeparators(repoDirObj.absolutePath());
    if (!prefix.endsWith(u'/')) {
      prefix.append(u'/');
    }
    return prefix;
  }();

  const auto allocateTargetPath = [&repoDirObj](const QFileInfo& sourceInfo) {
    const QString originalName = sourceInfo.fileName().isEmpty() ? QStringLiteral("mod") : sourceInfo.fileName();
    QString baseName = sourceInfo.completeBaseName();
    if (baseName.isEmpty()) {
      baseName = QStringLiteral("mod");
    }
    const QString suffix = sourceInfo.completeSuffix();

    QString candidateName = originalName;
    QString candidatePath = repoDirObj.filePath(candidateName);
    int counter = 1;
    while (QFileInfo::exists(candidatePath)) {
      if (suffix.isEmpty()) {
        candidateName = QStringLiteral("%1_%2").arg(baseName).arg(counter);
      } else {
        candidateName = QStringLiteral("%1_%2.%3").arg(baseName).arg(counter).arg(suffix);
      }
      candidatePath = repoDirObj.filePath(candidateName);
      ++counter;
    }
    return QDir::cleanPath(candidatePath);
  };

  const auto inRepo = [&repoPrefix](const QFileInfo& info) {
    const QString normalizedFile = QDir::fromNativeSeparators(info.absoluteFilePath());
    return normalizedFile.startsWith(repoPrefix, Qt::CaseInsensitive);
  };

  const auto performTransfer = [action, &errors](const QString& src, const QString& dst, const QString& label) {
    auto emitFailure = [&]() {
      errors << tr("无法%1 %2 到仓库目录：%3")
                    .arg(action == ImportAction::Cut ? tr("剪切") : tr("复制"))
                    .arg(label, dst);
      return false;
    };

    switch (action) {
      case ImportAction::Copy:
        if (!QFile::copy(src, dst)) {
          return emitFailure();
        }
        return true;
      case ImportAction::Cut:
        if (QFile::rename(src, dst)) {
          return true;
        }
        if (QFile::copy(src, dst)) {
          QFile::remove(src);
          return true;
        }
        return emitFailure();
      case ImportAction::None:
        return true;
    }
    return true;
  };

  const auto handlePath = [&](std::string& pathRef, const QString& label, bool required) {
    if (pathRef.empty()) {
      if (required) {
        errors << tr("%1路径为空，无法执行入库操作。").arg(label);
        return false;
      }
      return true;
    }

    QString sourcePath = QDir::cleanPath(QDir::fromNativeSeparators(QString::fromStdString(pathRef)));
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()) {
      if (required) {
        errors << tr("找不到%1：%2").arg(label, sourcePath);
        return false;
      }
      return true;
    }

    if (inRepo(sourceInfo)) {
      pathRef = QDir::toNativeSeparators(sourceInfo.absoluteFilePath()).toStdString();
      return true;
    }

    const QString targetPath = allocateTargetPath(sourceInfo);
    QFileInfo targetInfo(targetPath);
    if (!targetInfo.dir().exists() && !targetInfo.dir().mkpath(QStringLiteral("."))) {
      errors << tr("无法创建目标目录：%1").arg(targetInfo.dir().absolutePath());
      return false;
    }

    if (!performTransfer(sourceInfo.absoluteFilePath(), targetPath, label)) {
      return false;
    }

    pathRef = QDir::toNativeSeparators(targetPath).toStdString();
    return true;
  };

  const bool fileOk = handlePath(mod.file_path, tr("MOD 文件"), true);
  const bool coverOk = handlePath(mod.cover_path, tr("封面文件"), false);
  return fileOk && coverOk;
}
void MainWindow::onRepositoryModsReloaded() {
  reloadRepoSelectorData();
}

