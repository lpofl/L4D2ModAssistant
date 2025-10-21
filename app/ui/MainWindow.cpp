#include "app/ui/MainWindow.h"

#include <algorithm>

#include <QComboBox>
#include <QDir>
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
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include "app/ui/ModEditorDialog.h"
#include "core/config/Settings.h"
#include "core/db/Db.h"
#include "core/db/Migrations.h"
#include "core/log/Log.h"

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
  setWindowTitle("L4D2 MOD 管理器");
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
  searchEdit_ = new QLineEdit(page);
  searchEdit_->setPlaceholderText(tr("搜索名称 / 来源 / 备注"));
  categoryFilter_ = new QComboBox(page);
  importBtn_ = new QPushButton(tr("导入"), page);
  filterRow->addWidget(searchEdit_, 2);
  filterRow->addWidget(categoryFilter_, 1);
  filterRow->addStretch(1);
  filterRow->addWidget(importBtn_);
  layout->addLayout(filterRow);

  auto* splitter = new QSplitter(Qt::Horizontal, page);

  // Left side: table and actions
  auto* leftPanel = new QWidget(splitter);
  auto* leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(8);

  modTable_ = new QTableWidget(leftPanel);
  modTable_->setColumnCount(8);
  modTable_->setHorizontalHeaderLabels(
      {tr("名称"), tr("分类"), tr("TAG"), tr("作者"), tr("评分"), tr("发布日期"), tr("来源"), tr("备注")});
  modTable_->horizontalHeader()->setStretchLastSection(true);
  modTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  modTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  modTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  modTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  modTable_->setAlternatingRowColors(true);
  modTable_->verticalHeader()->setVisible(false);

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

  // Right side: details
  auto* rightPanel = new QWidget(splitter);
  auto* rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(12);

  coverLabel_ = new QLabel(tr("选中的 MOD 的图片"), rightPanel);
  coverLabel_->setAlignment(Qt::AlignCenter);
  coverLabel_->setMinimumSize(280, 240);
  coverLabel_->setStyleSheet("QLabel { background: #1f5f7f; color: white; border-radius: 6px; }");

  metaLabel_ = new QLabel(rightPanel);
  metaLabel_->setWordWrap(true);

  noteView_ = new QTextEdit(rightPanel);
  noteView_->setReadOnly(true);
  noteView_->setPlaceholderText(tr("选中的 MOD 的完整备注"));

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
  connect(searchEdit_, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
  connect(categoryFilter_, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
  connect(importBtn_, &QPushButton::clicked, this, &MainWindow::onImport);
  connect(editBtn_, &QPushButton::clicked, this, &MainWindow::onEdit);
  connect(deleteBtn_, &QPushButton::clicked, this, &MainWindow::onDelete);
  connect(refreshBtn_, &QPushButton::clicked, this, &MainWindow::onRefresh);

  return page;
}

QWidget* MainWindow::buildSelectorPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->addStretch();
  layout->addWidget(new QLabel(tr("选择器功能开发中"), page), 0, Qt::AlignCenter);
  layout->addStretch();
  return page;
}

QWidget* MainWindow::buildSettingsPage() {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->addStretch();
  layout->addWidget(new QLabel(tr("设置功能开发中"), page), 0, Qt::AlignCenter);
  layout->addStretch();
  return page;
}

void MainWindow::reloadCategories() {
  categoryNames_.clear();
  categoryFilter_->blockSignals(true);
  categoryFilter_->clear();
  categoryFilter_->addItem(tr("全部分类"), 0);
  if (!repo_) {
    categoryFilter_->blockSignals(false);
    return;
  }
  const auto categories = repo_->listCategories();
  for (const auto& category : categories) {
    categoryNames_[category.id] = QString::fromStdString(category.name);
    categoryFilter_->addItem(QString::fromStdString(category.name), category.id);
  }
  categoryFilter_->blockSignals(false);
}

void MainWindow::loadData() {
  if (!repo_) return;
  mods_ = repo_->listVisible();
  modTagsText_.clear();
  for (const auto& mod : mods_) {
    modTagsText_[mod.id] = joinTags(repo_->listTagsForMod(mod.id));
  }
  populateTable();
}

void MainWindow::populateTable() {
  modTable_->setRowCount(0);
  const QString keyword = searchEdit_->text().trimmed();
  const int categoryId = categoryFilter_->currentData().toInt();

  int row = 0;
  for (const auto& mod : mods_) {
    if (categoryId > 0 && mod.category_id != categoryId) {
      continue;
    }

    const QString name = QString::fromStdString(mod.name);
    const QString author = toDisplay(mod.author);
    const QString source = toDisplay(mod.source);
    const QString note = toDisplay(mod.note);
    const QString tags = modTagsText_[mod.id];

    if (!keyword.isEmpty()) {
      const QString haystack = name + u' ' + author + u' ' + source + u' ' + note + u' ' + tags;
      if (!haystack.contains(keyword, Qt::CaseInsensitive)) {
        continue;
      }
    }

    modTable_->insertRow(row);
    auto* itemName = new QTableWidgetItem(name);
    itemName->setData(Qt::UserRole, mod.id);
    modTable_->setItem(row, 0, itemName);
    modTable_->setItem(row, 1, new QTableWidgetItem(categoryNameFor(mod.category_id)));
    modTable_->setItem(row, 2, new QTableWidgetItem(tags));
    modTable_->setItem(row, 3, new QTableWidgetItem(author));
    modTable_->setItem(row, 4, new QTableWidgetItem(mod.rating > 0 ? QString::number(mod.rating) : QString("-")));
    modTable_->setItem(row, 5, new QTableWidgetItem(toDisplay(mod.published_at, tr("-"))));
    modTable_->setItem(row, 6, new QTableWidgetItem(source));
    modTable_->setItem(row, 7, new QTableWidgetItem(note));
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
    coverLabel_->setText(tr("选中的 MOD 的图片"));
    metaLabel_->clear();
    noteView_->clear();
    return;
  }

  const ModRow& mod = *it;
  const QString categoryName = categoryNameFor(mod.category_id);
  const QString tags = modTagsText_[mod.id];

  // Cover rendering
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
    coverLabel_->setText(tr("暂无封面"));
  }

  noteView_->setPlainText(QString::fromStdString(mod.note));

  QStringList meta;
  meta << tr("名称: %1").arg(QString::fromStdString(mod.name));
  meta << tr("分类: %1").arg(categoryName.isEmpty() ? tr("未分类") : categoryName);
  meta << tr("TAG: %1").arg(tags.isEmpty() ? tr("无") : tags);
  meta << tr("作者: %1").arg(toDisplay(mod.author, tr("未知")));
  meta << tr("评分: %1").arg(mod.rating > 0 ? QString::number(mod.rating) : tr("未评分"));
  meta << tr("体积: %1 MB").arg(QString::number(mod.size_mb, 'f', 2));
  if (!mod.published_at.empty()) meta << tr("发布日期: %1").arg(QString::fromStdString(mod.published_at));
  if (!mod.source.empty()) meta << tr("来源: %1").arg(QString::fromStdString(mod.source));
  if (!mod.file_path.empty()) meta << tr("文件: %1").arg(QString::fromStdString(mod.file_path));
  if (!mod.file_hash.empty()) meta << tr("哈希: %1").arg(QString::fromStdString(mod.file_hash));
  meta << tr("创建时间: %1").arg(QString::fromStdString(mod.created_at));
  meta << tr("更新时间: %1").arg(QString::fromStdString(mod.updated_at));
  metaLabel_->setText(meta.join('\n'));
}

QString MainWindow::categoryNameFor(int categoryId) const {
  if (categoryId > 0) {
    const auto it = categoryNames_.find(categoryId);
    if (it != categoryNames_.end()) {
      return it->second;
    }
    return QStringLiteral("分类#%1").arg(categoryId);
  }
  return tr("未分类");
}

QString MainWindow::tagsTextForMod(int modId) {
  return modTagsText_[modId];
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
  repo_->createModWithTags(mod, dialog.selectedTags());
  loadData();
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
    QMessageBox::warning(this, tr("未找到"), tr("数据库中不存在该 MOD。"));
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

void MainWindow::onDelete() {
  auto* item = modTable_->currentItem();
  if (!item) return;
  const int modId = modTable_->item(modTable_->currentRow(), 0)->data(Qt::UserRole).toInt();

  const auto reply =
      QMessageBox::question(this, tr("确认删除"), tr("确认要在仓库中隐藏该 MOD 吗？"), QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes) return;

  repo_->setModDeleted(modId, true);
  loadData();
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
