#include "app/ui/MainWindow.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication> // For applicationDirPath()
#include <QDir>             // For mkpath()
#include <QSettings>        // For registry access
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QMap>
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
  Settings settings = Settings::loadOrCreate();

  bool settingsChanged = false;
  if (settings.repoDir.empty()) {
    QString appDirPath = QCoreApplication::applicationDirPath();
    QString defaultRepoPath = QDir(appDirPath).filePath("repo");
    settings.repoDir = defaultRepoPath.toStdString();
    settings.repoDbPath = settings.repoDir + "/repo.db"; // Update derived db path
    settingsChanged = true;
    spdlog::info("repoDir was empty, set to default: {}", settings.repoDir);
  }

  QDir repoDir(QString::fromStdString(settings.repoDir));
  if (!repoDir.exists()) {
    if (repoDir.mkpath(".")) { // Create the directory if it doesn't exist
      spdlog::info("Created repo directory: {}", settings.repoDir);
      settingsChanged = true;
    } else {
      spdlog::error("Failed to create repo directory: {}", settings.repoDir);
      // Handle error, maybe fallback or show message
    }
  }

  if (settingsChanged) {
    settings.save(); // Save settings if repoDir was modified or created
  }

  // Detect L4D2 game directory if empty
  if (settings.gameDirectory.empty()) {
    QString detectedPath = detectL4D2GameDirectory();
    if (!detectedPath.isEmpty()) {
      settings.gameDirectory = detectedPath.toStdString();
      settingsChanged = true;
      spdlog::info("Detected L4D2 game directory: {}", settings.gameDirectory);
    } else {
      spdlog::warn("Could not detect L4D2 game directory automatically.");
    }
  }

  if (settingsChanged) {
    settings.save(); // Save settings if gameDirectory was modified
  }

  repoDir_ = QString::fromStdString(settings.repoDir);
  spdlog::info("Repo DB: {}", settings.repoDbPath);

  auto db = std::make_shared<Db>(settings.repoDbPath);
  runMigrations(*db);
  spdlog::info("Schema ready, version {}", migrations::currentSchemaVersion(*db));
  repo_ = std::make_unique<RepositoryService>(db);

  reloadCategories();
  loadData();
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

  repoSelectorFilterModel_ = new QStandardItemModel(this);
  repoSelectorProxyModel_ = new QSortFilterProxyModel(this);
  repoSelectorProxyModel_->setSourceModel(repoSelectorFilterModel_);
  repoSelectorProxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);
  repoSelectorProxyModel_->setFilterKeyColumn(0);
  repoCategoryFilter_->setModel(repoSelectorProxyModel_);

  filterAttribute_->addItems({tr("名称"), tr("分类"), tr("标签"), tr("作者"), tr("评分")});
  filterAttribute_->setCurrentText(tr("名称"));
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

  auto* tablesLayout = new QHBoxLayout();
  tablesLayout->setSpacing(12);

  // Left panel (Game Directory)
  auto* leftPanel = new QWidget(page);
  auto* leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(8);

  auto* gameDirLabel = new QLabel(tr("游戏目录"), leftPanel);
  gameDirCategoryFilter_ = new QComboBox(leftPanel);
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
  leftLayout->addWidget(gameDirCategoryFilter_);
  leftLayout->addWidget(gameDirTable_);

  // Right panel (Repository)
  auto* rightPanel = new QWidget(page);
  auto* rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(8);

  auto* repoLabel = new QLabel(tr("仓库"), rightPanel);
  repoCategoryFilter_ = new QComboBox(rightPanel);
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
  rightLayout->addWidget(repoCategoryFilter_);
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

  connect(configureStrategyBtn_, &QPushButton::clicked, this, &MainWindow::onConfigureStrategy);
  connect(randomizeBtn_, &QPushButton::clicked, this, &MainWindow::onRandomize);
  connect(saveCombinationBtn_, &QPushButton::clicked, this, &MainWindow::onSaveCombination);
  connect(applyToGameBtn_, &QPushButton::clicked, this, &MainWindow::onApplyToGame);

  return page;
}

QWidget* MainWindow::buildSettingsPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(12);

  // Clear Deleted MOD Data Button
  clearDeletedModsBtn_ = new QPushButton(tr("清除已删除MOD数据记录"), page);
  layout->addWidget(clearDeletedModsBtn_);
  layout->addStretch(1); // Push button to top

  // Connect the button
  connect(clearDeletedModsBtn_, &QPushButton::clicked, this, &MainWindow::onClearDeletedMods);

  return page;
}

void MainWindow::reloadCategories() {
  categoryNames_.clear();
  categoryParent_.clear();

  auto* allItem = new QStandardItem(tr("全部分类"));
  allItem->setData(0, Qt::UserRole);
  filterModel_->appendRow(allItem);

  const auto categories = repo_->listCategories();
  std::vector<CategoryRow> topLevel;
  std::unordered_map<int, std::vector<CategoryRow>> children;

  for (const auto& category : categories) {
    categoryNames_[category.id] = QString::fromStdString(category.name);
    if (category.parent_id.has_value()) {
      const int parentId = *category.parent_id;
      categoryParent_[category.id] = parentId;
      children[parentId].push_back(category);
    } else {
      topLevel.push_back(category);
    }
  }

  for (const auto& parent : topLevel) {
    auto* parentItem = new QStandardItem(QString::fromStdString(parent.name));
    parentItem->setData(parent.id, Qt::UserRole);
    filterModel_->appendRow(parentItem);

    if (children.count(parent.id)) {
      for (const auto& child : children.at(parent.id)) {
        auto* childItem = new QStandardItem("  " + QString::fromStdString(child.name));
        childItem->setData(child.id, Qt::UserRole);
        filterModel_->appendRow(childItem);
      }
    }
  }
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
}

void MainWindow::populateTable() {
  modTable_->setRowCount(0);

  const QString filterAttribute = filterAttribute_->currentText();
  const QString filterValue = filterValue_->currentText();

  int filterId = 0;
  if (filterValue_->currentIndex() >= 0) {
    QModelIndex proxyIndex = proxyModel_->index(filterValue_->currentIndex(), 0);
    QModelIndex sourceIndex = proxyModel_->mapToSource(proxyIndex);
    filterId = filterModel_->data(sourceIndex, Qt::UserRole).toInt();
  }

  int row = 0;
  for (const auto& mod : mods_) {
    // Filter by deleted status
    if (!showDeletedModsCheckBox_->isChecked() && mod.is_deleted) {
      continue;
    }

    if (filterAttribute == tr("分类")) {
      if (filterId > 0 && !categoryMatchesFilter(mod.category_id, filterId)) {
        continue;
      }
    } else if (filterAttribute == tr("名称")) {
      const QString name = QString::fromStdString(mod.name);
      if (!name.contains(filterValue, Qt::CaseInsensitive)) {
        continue;
      }
    } else if (filterAttribute == tr("标签")) {
      if (filterId > 0) {
        const auto& modTags = modTagsCache_[mod.id];
        const auto it = std::find_if(modTags.begin(), modTags.end(), [filterId](const TagWithGroupRow& tag) {
          return tag.id == filterId;
        });
        if (it == modTags.end()) {
          continue;
        }
      }
    } else if (filterAttribute == tr("作者")) {
      if (filterValue != tr("全部作者") && !filterValue.isEmpty()) {
        if (QString::fromStdString(mod.author) != filterValue) {
          continue;
        }
      }
    } else if (filterAttribute == tr("评分")) {
      if (filterId != 0) {
        if (filterId > 0) {
          if (mod.rating != filterId) {
            continue;
          }
        } else { // filterId < 0 for unrated
          if (mod.rating > 0) {
            continue;
          }
        }
      }
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

  filterValue_->blockSignals(false); // Unblock signals here
  filterValue_->setCurrentIndex(attribute == tr("名称") ? -1 : 0); // Set index after model is populated

  onFilterChanged(); // Trigger filter update
}

void MainWindow::reloadRatings() {
  auto* allItem = new QStandardItem(tr("全部评分"));
  allItem->setData(0, Qt::UserRole);
  filterModel_->appendRow(allItem);

  for (int i = 5; i >= 1; --i) {
    auto* item = new QStandardItem(tr("%1 星").arg(i));
    item->setData(i, Qt::UserRole);
    filterModel_->appendRow(item);
  }

  auto* unratedItem = new QStandardItem(tr("未评分"));
  unratedItem->setData(-1, Qt::UserRole);
  filterModel_->appendRow(unratedItem);
}

void MainWindow::reloadAuthors() {
  auto* allItem = new QStandardItem(tr("全部作者"));
  allItem->setData("", Qt::UserRole);
  filterModel_->appendRow(allItem);

  QSet<QString> authors;
  for (const auto& mod : mods_) {
    authors.insert(QString::fromStdString(mod.author));
  }

  for (const auto& author : authors) {
    auto* authorItem = new QStandardItem(author);
    authorItem->setData(author, Qt::UserRole);
    filterModel_->appendRow(authorItem);
  }
}

void MainWindow::reloadTags() {
  auto* allItem = new QStandardItem(tr("全部标签"));
  allItem->setData(0, Qt::UserRole);
  filterModel_->appendRow(allItem);

  const auto tags = repo_->listTags();
  QMap<QString, QList<TagWithGroupRow>> groupedTags;
  for (const auto& tag : tags) {
    groupedTags[QString::fromStdString(tag.group_name)].append(tag);
  }

  for (auto it = groupedTags.constBegin(); it != groupedTags.constEnd(); ++it) {
    auto* groupItem = new QStandardItem(it.key());
    groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsSelectable);
    filterModel_->appendRow(groupItem);

    for (const auto& tag : it.value()) {
      auto* tagItem = new QStandardItem("  " + QString::fromStdString(tag.name));
      tagItem->setData(tag.id, Qt::UserRole);
      filterModel_->appendRow(tagItem);
    }
  }
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
  reloadRepoSelectorData();
}

void MainWindow::reloadRepoSelectorData() {
  // Populate categories
  repoSelectorFilterModel_->clear();
  auto* allItem = new QStandardItem(tr("全部分类"));
  allItem->setData(0, Qt::UserRole);
  repoSelectorFilterModel_->appendRow(allItem);

  const auto categories = repo_->listCategories();
  std::vector<CategoryRow> topLevel;
  std::unordered_map<int, std::vector<CategoryRow>> children;

  for (const auto& category : categories) {
    if (category.parent_id.has_value()) {
      children[*category.parent_id].push_back(category);
    } else {
      topLevel.push_back(category);
    }
  }

  for (const auto& parent : topLevel) {
    auto* parentItem = new QStandardItem(QString::fromStdString(parent.name));
    parentItem->setData(parent.id, Qt::UserRole);
    repoSelectorFilterModel_->appendRow(parentItem);

    if (children.count(parent.id)) {
      for (const auto& child : children.at(parent.id)) {
        auto* childItem = new QStandardItem("  " + QString::fromStdString(child.name));
        childItem->setData(child.id, Qt::UserRole);
        repoSelectorFilterModel_->appendRow(childItem);
      }
    }
  }

  // Populate mods table
  repoTable_->setRowCount(0);
  int row = 0;
  for (const auto& mod : mods_) {
    repoTable_->insertRow(row);
    auto* itemName = new QTableWidgetItem(QString::fromStdString(mod.name));
    itemName->setData(Qt::UserRole, mod.id);
    repoTable_->setItem(row, 0, itemName);
    repoTable_->setItem(row, 1, new QTableWidgetItem(modTagsText_[mod.id]));
    repoTable_->setItem(row, 2, new QTableWidgetItem(toDisplay(mod.author)));
    repoTable_->setItem(row, 3, new QTableWidgetItem(mod.rating > 0 ? QString::number(mod.rating) : QString("-")));
    repoTable_->setItem(row, 4, new QTableWidgetItem(toDisplay(mod.note)));
    ++row;
  }
}

void MainWindow::switchToSettings() {
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
