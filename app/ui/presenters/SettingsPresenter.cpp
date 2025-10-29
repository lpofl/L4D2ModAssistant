#include "app/ui/presenters/SettingsPresenter.h"

#include <algorithm>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>

#include "app/ui/pages/SettingsPage.h"
#include "core/config/Settings.h"
#include "core/repo/CategoryDao.h"
#include "core/repo/RepositoryService.h"
#include "core/repo/TagDao.h"

namespace {

QString toDisplay(const std::string& text, const QString& fallback = {}) {
  return text.empty() ? fallback : QString::fromStdString(text);
}

} // namespace

SettingsPresenter::SettingsPresenter(SettingsPage* page,
                                     Settings& settings,
                                     RepositoryService* repo,
                                     QWidget* dialogParent,
                                     std::function<void()> repositoryReload,
                                     QObject* parent)
    : QObject(parent),
      page_(page),
      settings_(&settings),
      repo_(repo),
      dialogParent_(dialogParent),
      repositoryReloadCallback_(std::move(repositoryReload)) {
  if (!page_) {
    return;
  }

  auto* nav = page_->navigation();
  auto* repoBrowseBtn = page_->repoBrowseButton();
  auto* gameBrowseBtn = page_->gameDirBrowseButton();
  auto* gameDirEdit = page_->gameDirEdit();
  auto* importCombo = page_->importModeCombo();
  auto* autoImportCheck = page_->autoImportCheck();
  auto* autoImportCombo = page_->autoImportModeCombo();
  auto* saveBtn = page_->saveSettingsButton();
  auto* categoryTree = page_->categoryTree();
  auto* categoryAddRoot = page_->categoryAddRootButton();
  auto* categoryAddChild = page_->categoryAddChildButton();
  auto* categoryRename = page_->categoryRenameButton();
  auto* categoryDelete = page_->categoryDeleteButton();
  auto* categoryMoveUp = page_->categoryMoveUpButton();
  auto* categoryMoveDown = page_->categoryMoveDownButton();
  auto* tagGroupList = page_->tagGroupList();
  auto* tagList = page_->tagList();
  auto* tagGroupAdd = page_->tagGroupAddButton();
  auto* tagGroupRename = page_->tagGroupRenameButton();
  auto* tagGroupDelete = page_->tagGroupDeleteButton();
  auto* tagAdd = page_->tagAddButton();
  auto* tagRename = page_->tagRenameButton();
  auto* tagDelete = page_->tagDeleteButton();
  auto* clearDeletedBtn = page_->clearDeletedButton();

  connect(nav, &QListWidget::currentRowChanged, this, &SettingsPresenter::onSettingsNavChanged);
  connect(repoBrowseBtn, &QPushButton::clicked, this, &SettingsPresenter::onBrowseRepoDir);
  connect(gameBrowseBtn, &QPushButton::clicked, this, &SettingsPresenter::onBrowseGameDir);
  connect(gameDirEdit, &QLineEdit::textChanged, this, &SettingsPresenter::onGameDirEdited);
  connect(importCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPresenter::onImportModeChanged);
  connect(autoImportCheck, &QCheckBox::toggled, this, &SettingsPresenter::onAutoImportToggled);
  connect(autoImportCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsPresenter::onAutoImportModeChanged);
  connect(saveBtn, &QPushButton::clicked, this, &SettingsPresenter::onSaveSettings);

  connect(categoryTree, &QTreeWidget::itemSelectionChanged, this, &SettingsPresenter::onCategorySelectionChanged);
  connect(categoryTree, &QTreeWidget::itemChanged, this, &SettingsPresenter::onCategoryItemChanged);
  connect(categoryAddRoot, &QPushButton::clicked, this, &SettingsPresenter::onAddCategoryTopLevel);
  connect(categoryAddChild, &QPushButton::clicked, this, &SettingsPresenter::onAddCategoryChild);
  connect(categoryRename, &QPushButton::clicked, this, &SettingsPresenter::onRenameCategory);
  connect(categoryDelete, &QPushButton::clicked, this, &SettingsPresenter::onDeleteCategory);
  connect(categoryMoveUp, &QPushButton::clicked, this, &SettingsPresenter::onMoveCategoryUp);
  connect(categoryMoveDown, &QPushButton::clicked, this, &SettingsPresenter::onMoveCategoryDown);

  connect(tagGroupList, &QListWidget::currentRowChanged, this, &SettingsPresenter::onTagGroupSelectionChanged);
  connect(tagGroupAdd, &QPushButton::clicked, this, &SettingsPresenter::onAddTagGroup);
  connect(tagGroupRename, &QPushButton::clicked, this, &SettingsPresenter::onRenameTagGroup);
  connect(tagGroupDelete, &QPushButton::clicked, this, &SettingsPresenter::onDeleteTagGroup);
  connect(tagList, &QListWidget::currentRowChanged, this, &SettingsPresenter::onTagSelectionChanged);
  connect(tagAdd, &QPushButton::clicked, this, &SettingsPresenter::onAddTag);
  connect(tagRename, &QPushButton::clicked, this, &SettingsPresenter::onRenameTag);
  connect(tagDelete, &QPushButton::clicked, this, &SettingsPresenter::onDeleteTag);

  connect(clearDeletedBtn, &QPushButton::clicked, this, &SettingsPresenter::onClearDeletedMods);

  refreshAll();
}

void SettingsPresenter::setRepositoryService(RepositoryService* repo) {
  repo_ = repo;
}

void SettingsPresenter::refreshAll() {
  refreshBasicSettingsUi();
  refreshCategoryManagementUi();
  refreshTagManagementUi();
  refreshDeletionSettingsUi();
  ensureSettingsNavSelection();
}

void SettingsPresenter::onSettingsNavChanged(int row) {
  if (auto* stack = page_->stack()) {
    if (row >= 0 && row < stack->count()) {
      stack->setCurrentIndex(row);
    }
  }
  setSettingsStatus({});
}

void SettingsPresenter::onBrowseRepoDir() {
  if (!page_) {
    return;
  }
  auto* repoEdit = page_->repoDirEdit();
  if (!repoEdit) {
    return;
  }
  const QString current = repoEdit->text();
  const QString defaultPath = QString::fromStdString(settings_->repoDir);
  QString selected = QFileDialog::getExistingDirectory(dialogParent_,
                                                       tr("选择仓库目录"),
                                                       current.isEmpty() ? defaultPath : current);
  if (!selected.isEmpty()) {
    repoEdit->setText(QDir::cleanPath(selected));
    setSettingsStatus({});
  }
}

void SettingsPresenter::onBrowseGameDir() {
  auto* gameEdit = page_->gameDirEdit();
  if (!gameEdit) {
    return;
  }
  const QString current = gameEdit->text();
  QString selected = QFileDialog::getExistingDirectory(dialogParent_, tr("选择游戏根目录"), current);
  if (!selected.isEmpty()) {
    const QString cleanedRoot = normalizeRootInput(selected);
    gameEdit->setText(QDir::toNativeSeparators(cleanedRoot));
    updateDerivedGamePaths(cleanedRoot);
    setSettingsStatus({});
  }
}

void SettingsPresenter::onGameDirEdited(const QString& path) {
  updateDerivedGamePaths(normalizeRootInput(path));
  setSettingsStatus({});
}

void SettingsPresenter::onImportModeChanged(int /*index*/) {
  setSettingsStatus({});
}

void SettingsPresenter::onAutoImportToggled(bool checked) {
  if (auto* combo = page_->autoImportModeCombo()) {
    combo->setEnabled(checked);
  }
  setSettingsStatus({});
}

void SettingsPresenter::onAutoImportModeChanged(int /*index*/) {
  setSettingsStatus({});
}

void SettingsPresenter::onSaveSettings() {
  if (!page_) {
    return;
  }

  Settings updated = *settings_;
  const QString appDirPath = QCoreApplication::applicationDirPath();
  const QString databaseDirPath = QDir(appDirPath).filePath(QStringLiteral("database"));

  auto* repoEdit = page_->repoDirEdit();
  auto* gameEdit = page_->gameDirEdit();
  auto* importCombo = page_->importModeCombo();
  auto* autoImportCheck = page_->autoImportCheck();
  auto* autoImportCombo = page_->autoImportModeCombo();
  auto* retainDeleted = page_->retainDeletedCheck();

  if (repoEdit) {
    const QString repoPath = QDir::cleanPath(QDir::fromNativeSeparators(repoEdit->text().trimmed()));
    if (repoPath.isEmpty()) {
      setSettingsStatus(tr("仓库目录不能为空"), true);
      return;
    }
    QDir repoDir(repoPath);
    if (!repoDir.exists() && !repoDir.mkpath(QStringLiteral("."))) {
      setSettingsStatus(tr("无法创建仓库目录"), true);
      return;
    }
    updated.repoDir = repoPath.toStdString();
  }

  if (gameEdit) {
    const QString rootPath = normalizeRootInput(gameEdit->text());
    if (!rootPath.isEmpty()) {
      updated.gameDirectory = rootPath.toStdString();
      const QString addons = deriveAddonsPath(rootPath);
      updated.addonsPath = QDir::toNativeSeparators(addons).toStdString();
      updated.workshopPath = QDir::toNativeSeparators(deriveWorkshopPath(addons)).toStdString();
    } else {
      updated.gameDirectory.clear();
      updated.addonsPath.clear();
      updated.workshopPath.clear();
    }
  }

  if (importCombo) {
    updated.importAction = static_cast<ImportAction>(importCombo->currentData().toInt());
  }

  if (autoImportCheck) {
    updated.addonsAutoImportEnabled = autoImportCheck->isChecked();
  }
  if (autoImportCombo) {
    updated.addonsAutoImportMethod = static_cast<AddonsAutoImportMethod>(autoImportCombo->currentData().toInt());
  }

  if (retainDeleted) {
    updated.retainDataOnDelete = retainDeleted->isChecked();
  }

  QDir dbDir(databaseDirPath);
  if (!dbDir.exists() && !dbDir.mkpath(QStringLiteral("."))) {
    setSettingsStatus(tr("无法创建数据库目录"), true);
    return;
  }
  updated.repoDbPath = QDir(appDirPath).filePath("database/repo.db").toStdString();

  try {
    updated.save();
    const bool repoChanged = updated.repoDir != settings_->repoDir;
    *settings_ = updated;
    setSettingsStatus(tr("设置已保存"));

    if (repoChanged) {
      emit requestRepositoryReinitialize();
    } else if (repositoryReloadCallback_) {
      repositoryReloadCallback_();
    }
  } catch (const std::exception& ex) {
    setSettingsStatus(QString::fromUtf8(ex.what()), true);
  }
}

void SettingsPresenter::onClearDeletedMods() {
  if (!repo_) {
    return;
  }
  const auto reply = QMessageBox::question(dialogParent_,
                                           tr("清理确认"),
                                           tr("确定要永久清除已删除的 MOD 记录吗？"),
                                           QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }
  try {
    repo_->clearDeletedMods();
    setSettingsStatus(tr("已清理所有标记删除的 MOD"));
    if (repositoryReloadCallback_) {
      repositoryReloadCallback_();
    }
  } catch (const std::exception& ex) {
    setSettingsStatus(QString::fromUtf8(ex.what()), true);
  }
}

void SettingsPresenter::refreshBasicSettingsUi() {
  auto* repoEdit = page_->repoDirEdit();
  auto* gameEdit = page_->gameDirEdit();
  auto* importCombo = page_->importModeCombo();
  auto* autoImportCheck = page_->autoImportCheck();
  auto* autoImportCombo = page_->autoImportModeCombo();

  if (repoEdit) {
    QSignalBlocker blocker(repoEdit);
    repoEdit->setText(QString::fromStdString(settings_->repoDir));
  }
  if (gameEdit) {
    QSignalBlocker blocker(gameEdit);
    const QString rootPath = QString::fromStdString(settings_->gameDirectory);
    gameEdit->setText(QDir::toNativeSeparators(rootPath));
    updateDerivedGamePaths(rootPath);
  }
  if (importCombo) {
    QSignalBlocker blocker(importCombo);
    const int index = importCombo->findData(static_cast<int>(settings_->importAction));
    importCombo->setCurrentIndex(index >= 0 ? index : 0);
  }
  if (autoImportCheck) {
    QSignalBlocker blocker(autoImportCheck);
    autoImportCheck->setChecked(settings_->addonsAutoImportEnabled);
  }
  if (autoImportCombo) {
    QSignalBlocker blocker(autoImportCombo);
    const int index = autoImportCombo->findData(static_cast<int>(settings_->addonsAutoImportMethod));
    autoImportCombo->setCurrentIndex(index >= 0 ? index : 0);
    autoImportCombo->setEnabled(settings_->addonsAutoImportEnabled);
  }

  setSettingsStatus({});
}

// ... (file truncated for brevity in script)

void SettingsPresenter::refreshCategoryManagementUi() {
  auto* tree = page_->categoryTree();
  if (!tree || !repo_) {
    return;
  }

  const int previousId = selectedCategoryId();
  QSignalBlocker blocker(tree);
  suppressCategoryItemSignals_ = true;
  tree->clear();

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
    std::sort(entry.second.begin(), entry.second.end(), compare);
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
      tree->addTopLevelItem(item);
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

  tree->expandAll();
  suppressCategoryItemSignals_ = false;

  if (previousId > 0) {
    QTreeWidgetItemIterator it(tree);
    while (*it) {
      if ((*it)->data(0, Qt::UserRole).toInt() == previousId) {
        tree->setCurrentItem(*it);
        break;
      }
      ++it;
    }
  }

  onCategorySelectionChanged();
}

void SettingsPresenter::refreshTagManagementUi() {
  auto* groupList = page_->tagGroupList();
  if (!groupList || !repo_) {
    return;
  }

  const int previousGroup = selectedTagGroupId();
  QSignalBlocker blocker(groupList);
  groupList->clear();

  const auto groups = repo_->listTagGroups();
  for (const auto& group : groups) {
    auto* item = new QListWidgetItem(QString::fromStdString(group.name));
    item->setData(Qt::UserRole, group.id);
    groupList->addItem(item);
  }

  int rowToSelect = 0;
  if (previousGroup > 0) {
    for (int row = 0; row < groupList->count(); ++row) {
      if (groupList->item(row)->data(Qt::UserRole).toInt() == previousGroup) {
        rowToSelect = row;
        break;
      }
    }
  }

  if (groupList->count() > 0) {
    groupList->setCurrentRow(rowToSelect);
  } else {
    groupList->setCurrentRow(-1);
  }

  refreshTagListForGroup(selectedTagGroupId());
}

void SettingsPresenter::refreshTagListForGroup(int groupId) {
  auto* tagList = page_->tagList();
  if (!tagList || !repo_) {
    return;
  }

  const int previousTag = selectedTagId();
  QSignalBlocker blocker(tagList);
  tagList->clear();

  if (groupId > 0) {
    const auto tags = repo_->listTagsByGroup(groupId);
    for (const auto& tag : tags) {
      auto* item = new QListWidgetItem(QString::fromStdString(tag.name));
      item->setData(Qt::UserRole, tag.id);
      tagList->addItem(item);
    }
  }

  int rowToSelect = 0;
  if (previousTag > 0) {
    for (int row = 0; row < tagList->count(); ++row) {
      if (tagList->item(row)->data(Qt::UserRole).toInt() == previousTag) {
        rowToSelect = row;
        break;
      }
    }
  }

  if (tagList->count() > 0) {
    tagList->setCurrentRow(rowToSelect);
  } else {
    tagList->setCurrentRow(-1);
  }

  onTagSelectionChanged(tagList->currentRow());
}

void SettingsPresenter::refreshDeletionSettingsUi() {
  if (auto* retain = page_->retainDeletedCheck()) {
    QSignalBlocker blocker(retain);
    retain->setChecked(settings_->retainDataOnDelete);
  }
}

void SettingsPresenter::ensureSettingsNavSelection() {
  if (auto* nav = page_->navigation()) {
    if (nav->currentRow() < 0 && nav->count() > 0) {
      nav->setCurrentRow(0);
    }
  }
}

void SettingsPresenter::setSettingsStatus(const QString& text, bool isError) {
  if (auto* label = page_->statusLabel()) {
    label->setText(text);
    label->setStyleSheet(isError ? QStringLiteral("color: #c62828;")
                                 : QStringLiteral("color: #2e7d32;"));
  }
}

void SettingsPresenter::onCategorySelectionChanged() {
  auto* tree = page_->categoryTree();
  if (!tree) {
    return;
  }

  const bool hasSelection = selectedCategoryId() > 0;
  if (auto* addChild = page_->categoryAddChildButton()) addChild->setEnabled(hasSelection);
  if (auto* rename = page_->categoryRenameButton()) rename->setEnabled(hasSelection);
  if (auto* remove = page_->categoryDeleteButton()) remove->setEnabled(hasSelection);

  bool canMoveUp = false;
  bool canMoveDown = false;
  if (hasSelection) {
    if (auto* item = tree->currentItem()) {
      QTreeWidgetItem* parent = item->parent();
      const int index = parent ? parent->indexOfChild(item) : tree->indexOfTopLevelItem(item);
      const int siblingCount = parent ? parent->childCount() : tree->topLevelItemCount();
      if (index >= 0) {
        canMoveUp = index > 0;
        canMoveDown = index + 1 < siblingCount;
      }
    }
  }

  if (auto* up = page_->categoryMoveUpButton()) up->setEnabled(canMoveUp);
  if (auto* down = page_->categoryMoveDownButton()) down->setEnabled(canMoveDown);
}

void SettingsPresenter::onCategoryItemChanged(QTreeWidgetItem* item, int column) {
  if (!repo_ || !item || suppressCategoryItemSignals_) {
    return;
  }

  const int id = item->data(0, Qt::UserRole).toInt();
  if (id <= 0) {
    return;
  }

  const auto restoreName = [&]() {
    suppressCategoryItemSignals_ = true;
    item->setText(0, item->data(0, Qt::UserRole + 2).toString());
    suppressCategoryItemSignals_ = false;
  };
  const auto restorePriority = [&]() {
    suppressCategoryItemSignals_ = true;
    const int priority = item->data(1, Qt::UserRole).toInt();
    item->setText(1, QString::number(priority));
    suppressCategoryItemSignals_ = false;
  };

  if (column == 0) {
    const QString newName = item->text(0).trimmed();
    if (newName.isEmpty()) {
      restoreName();
      return;
    }
    if (newName == item->data(0, Qt::UserRole + 2).toString()) {
      item->setText(0, newName);
      return;
    }
    try {
      const auto parentId = item->data(0, Qt::UserRole + 1).toInt();
      repo_->updateCategory(id, newName.toStdString(), parentId > 0 ? std::optional<int>(parentId) : std::nullopt, std::nullopt);
      item->setData(0, Qt::UserRole + 2, newName);
      if (repositoryReloadCallback_) {
        repositoryReloadCallback_();
      }
    } catch (const std::exception& ex) {
      restoreName();
      QMessageBox::warning(dialogParent_, tr("更新失败"), QString::fromUtf8(ex.what()));
    }
  } else if (column == 1) {
    bool ok = false;
    const int newPriority = item->text(1).trimmed().toInt(&ok);
    if (!ok || newPriority <= 0) {
      restorePriority();
      return;
    }
    if (newPriority == item->data(1, Qt::UserRole).toInt()) {
      item->setText(1, QString::number(newPriority));
      return;
    }
    try {
      const auto parentId = item->data(0, Qt::UserRole + 1).toInt();
      const QString currentName = item->data(0, Qt::UserRole + 2).toString();
      repo_->updateCategory(id, currentName.toStdString(), parentId > 0 ? std::optional<int>(parentId) : std::nullopt, newPriority);
      item->setData(1, Qt::UserRole, newPriority);
      if (repositoryReloadCallback_) {
        repositoryReloadCallback_();
      }
    } catch (const std::exception& ex) {
      restorePriority();
      QMessageBox::warning(dialogParent_, tr("更新失败"), QString::fromUtf8(ex.what()));
    }
  }
}

int SettingsPresenter::selectedCategoryId() const {
  if (auto* tree = page_->categoryTree()) {
    if (auto* item = tree->currentItem()) {
      return item->data(0, Qt::UserRole).toInt();
    }
  }
  return 0;
}

int SettingsPresenter::selectedCategoryParentId(int categoryId) const {
  if (auto* tree = page_->categoryTree()) {
    QTreeWidgetItemIterator it(tree);
    while (*it) {
      if ((*it)->data(0, Qt::UserRole).toInt() == categoryId) {
        return (*it)->data(0, Qt::UserRole + 1).toInt();
      }
      ++it;
    }
  }
  return 0;
}

void SettingsPresenter::onAddCategoryTopLevel() {
  if (!repo_) {
    return;
  }
  bool ok = false;
  const QString name = QInputDialog::getText(dialogParent_, tr("新增类别"), tr("类别名称"), QLineEdit::Normal, {}, &ok).trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  try {
    repo_->createCategory(name.toStdString(), std::nullopt);
    refreshCategoryManagementUi();
    if (repositoryReloadCallback_) {
      repositoryReloadCallback_();
    }
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("新增失败"), QString::fromUtf8(ex.what()));
  }
}

void SettingsPresenter::onAddCategoryChild() {
  const int parentId = selectedCategoryId();
  if (parentId <= 0 || !repo_) {
    return;
  }
  bool ok = false;
  const QString name = QInputDialog::getText(dialogParent_, tr("新增子类别"), tr("类别名称"), QLineEdit::Normal, {}, &ok).trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  try {
    repo_->createCategory(name.toStdString(), parentId);
    refreshCategoryManagementUi();
    if (repositoryReloadCallback_) {
      repositoryReloadCallback_();
    }
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("新增失败"), QString::fromUtf8(ex.what()));
  }
}

void SettingsPresenter::onRenameCategory() {
  const int id = selectedCategoryId();
  if (id <= 0 || !repo_) {
    return;
  }
  auto* tree = page_->categoryTree();
  if (!tree) {
    return;
  }
  auto* item = tree->currentItem();
  if (!item) {
    return;
  }
  const QString originalName = item->data(0, Qt::UserRole + 2).toString();
  bool ok = false;
  const QString name = QInputDialog::getText(dialogParent_, tr("重命名类别"), tr("类别名称"), QLineEdit::Normal, originalName, &ok).trimmed();
  if (!ok || name.isEmpty() || name == originalName) {
    return;
  }
  try {
    const int parentId = item->data(0, Qt::UserRole + 1).toInt();
    repo_->updateCategory(id, name.toStdString(), parentId > 0 ? std::optional<int>(parentId) : std::nullopt, std::nullopt);
    refreshCategoryManagementUi();
    if (repositoryReloadCallback_) {
      repositoryReloadCallback_();
    }
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("更新失败"), QString::fromUtf8(ex.what()));
  }
}

void SettingsPresenter::onDeleteCategory() {
  const int id = selectedCategoryId();
  if (id <= 0 || !repo_) {
    return;
  }
  const auto reply = QMessageBox::question(dialogParent_,
                                           tr("删除类别"),
                                           tr("确定要删除该类别及其子类别吗？"),
                                           QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }
  try {
    repo_->deleteCategory(id);
    refreshCategoryManagementUi();
    if (repositoryReloadCallback_) {
      repositoryReloadCallback_();
    }
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("删除失败"), QString::fromUtf8(ex.what()));
  }
}

void SettingsPresenter::adjustCategoryOrder(int direction) {
  if (!repo_ || direction == 0) {
    return;
  }
  auto* tree = page_->categoryTree();
  if (!tree) {
    return;
  }
  auto* item = tree->currentItem();
  if (!item) {
    return;
  }

  QTreeWidgetItem* parent = item->parent();
  const int index = parent ? parent->indexOfChild(item) : tree->indexOfTopLevelItem(item);
  const int siblingCount = parent ? parent->childCount() : tree->topLevelItemCount();
  const int targetIndex = index + direction;
  if (index < 0 || targetIndex < 0 || targetIndex >= siblingCount) {
    return;
  }

  QTreeWidgetItem* sibling = parent ? parent->child(targetIndex) : tree->topLevelItem(targetIndex);
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
    if (repositoryReloadCallback_) {
      repositoryReloadCallback_();
    }
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("调整失败"), QString::fromUtf8(ex.what()));
  }
}

void SettingsPresenter::onMoveCategoryUp() {
  adjustCategoryOrder(-1);
}

void SettingsPresenter::onMoveCategoryDown() {
  adjustCategoryOrder(1);
}

void SettingsPresenter::onTagGroupSelectionChanged(int row) {
  const bool hasSelection = row >= 0;
  if (auto* rename = page_->tagGroupRenameButton()) rename->setEnabled(hasSelection);
  if (auto* remove = page_->tagGroupDeleteButton()) remove->setEnabled(hasSelection);

  const int groupId = selectedTagGroupId();
  refreshTagListForGroup(groupId);
}

void SettingsPresenter::onAddTagGroup() {
  if (!repo_) {
    return;
  }
  bool ok = false;
  const QString name = QInputDialog::getText(dialogParent_, tr("新增标签组"), tr("标签组名称"), QLineEdit::Normal, {}, &ok).trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  try {
    repo_->createTagGroup(name.toStdString());
    refreshTagManagementUi();
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("新增失败"), QString::fromUtf8(ex.what()));
  }
}

void SettingsPresenter::onRenameTagGroup() {
  if (!repo_) {
    return;
  }
  const int groupId = selectedTagGroupId();
  if (groupId <= 0) {
    return;
  }
  auto* list = page_->tagGroupList();
  if (!list) {
    return;
  }
  auto* item = list->currentItem();
  if (!item) {
    return;
  }
  bool ok = false;
  const QString currentName = item->text();
  const QString name = QInputDialog::getText(dialogParent_, tr("重命名标签组"), tr("标签组名称"), QLineEdit::Normal, currentName, &ok).trimmed();
  if (!ok || name.isEmpty() || name == currentName) {
    return;
  }
  try {
    repo_->renameTagGroup(groupId, name.toStdString());
    refreshTagManagementUi();
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("更新失败"), QString::fromUtf8(ex.what()));
  }
}

void SettingsPresenter::onDeleteTagGroup() {
  if (!repo_) {
    return;
  }
  const int groupId = selectedTagGroupId();
  if (groupId <= 0) {
    return;
  }
  const auto reply = QMessageBox::question(dialogParent_,
                                           tr("删除标签组"),
                                           tr("确定要删除该标签组及其下所有标签吗？"),
                                           QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }
  try {
    repo_->deleteTagGroup(groupId);
    refreshTagManagementUi();
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("删除失败"), QString::fromUtf8(ex.what()));
  }
}

int SettingsPresenter::selectedTagGroupId() const {
  if (auto* list = page_->tagGroupList()) {
    if (auto* item = list->currentItem()) {
      return item->data(Qt::UserRole).toInt();
    }
  }
  return 0;
}

int SettingsPresenter::selectedTagId() const {
  if (auto* list = page_->tagList()) {
    if (auto* item = list->currentItem()) {
      return item->data(Qt::UserRole).toInt();
    }
  }
  return 0;
}

void SettingsPresenter::onTagSelectionChanged(int row) {
  const bool hasSelection = row >= 0;
  if (auto* rename = page_->tagRenameButton()) rename->setEnabled(hasSelection);
  if (auto* remove = page_->tagDeleteButton()) remove->setEnabled(hasSelection);
}

void SettingsPresenter::onAddTag() {
  if (!repo_) {
    return;
  }
  const int groupId = selectedTagGroupId();
  if (groupId <= 0) {
    QMessageBox::information(dialogParent_, tr("提示"), tr("请先选择一个标签组。"));
    return;
  }
  bool ok = false;
  const QString name = QInputDialog::getText(dialogParent_, tr("新增标签"), tr("标签名称"), QLineEdit::Normal, {}, &ok).trimmed();
  if (!ok || name.isEmpty()) {
    return;
  }
  try {
    repo_->createTag(groupId, name.toStdString());
    refreshTagListForGroup(groupId);
    if (repositoryReloadCallback_) {
      repositoryReloadCallback_();
    }
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("新增失败"), QString::fromUtf8(ex.what()));
  }
}

void SettingsPresenter::onRenameTag() {
  if (!repo_) {
    return;
  }
  const int tagId = selectedTagId();
  if (tagId <= 0) {
    return;
  }
  auto* list = page_->tagList();
  if (!list) {
    return;
  }
  auto* item = list->currentItem();
  if (!item) {
    return;
  }
  bool ok = false;
  const QString currentName = item->text();
  const QString name = QInputDialog::getText(dialogParent_, tr("重命名标签"), tr("标签名称"), QLineEdit::Normal, currentName, &ok).trimmed();
  if (!ok || name.isEmpty() || name == currentName) {
    return;
  }
  try {
    repo_->renameTag(tagId, name.toStdString());
    refreshTagListForGroup(selectedTagGroupId());
    if (repositoryReloadCallback_) {
      repositoryReloadCallback_();
    }
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("更新失败"), QString::fromUtf8(ex.what()));
  }
}

void SettingsPresenter::onDeleteTag() {
  if (!repo_) {
    return;
  }
  const int tagId = selectedTagId();
  if (tagId <= 0) {
    return;
  }
  const auto reply = QMessageBox::question(dialogParent_,
                                           tr("删除标签"),
                                           tr("确定要删除该标签吗？"),
                                           QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }
  try {
    repo_->deleteTag(tagId);
    refreshTagListForGroup(selectedTagGroupId());
    if (repositoryReloadCallback_) {
      repositoryReloadCallback_();
    }
  } catch (const std::exception& ex) {
    QMessageBox::warning(dialogParent_, tr("删除失败"), QString::fromUtf8(ex.what()));
  }
}

QString SettingsPresenter::normalizeRootInput(const QString& rawPath) const {
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

QString SettingsPresenter::deriveAddonsPath(const QString& rootPath) const {
  QDir root(rootPath);
  return QDir::cleanPath(root.filePath(QStringLiteral("left4dead2/addons")));
}

QString SettingsPresenter::deriveWorkshopPath(const QString& addonsPath) const {
  QDir addons(addonsPath);
  return QDir::cleanPath(addons.filePath(QStringLiteral("workshop")));
}

void SettingsPresenter::updateDerivedGamePaths(const QString& rootPath) {
  const QString addons = rootPath.isEmpty() ? QString() : QDir::toNativeSeparators(deriveAddonsPath(rootPath));
  const QString workshop = rootPath.isEmpty() ? QString() : QDir::toNativeSeparators(deriveWorkshopPath(addons));

  if (auto* addonsEdit = page_->addonsDisplay()) {
    addonsEdit->setText(addons);
  }
  if (auto* workshopEdit = page_->workshopDisplay()) {
    workshopEdit->setText(workshop);
  }
}
