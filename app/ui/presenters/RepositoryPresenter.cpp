#include "app/ui/presenters/RepositoryPresenter.h"

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <tuple>

#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPixmap>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QRegularExpression>

#include "app/services/ImportService.h"
#include "app/ui/ImportFolderDialog.h"
#include "app/ui/ModEditorDialog.h"
#include "app/ui/components/ModFilterPanel.h"
#include "app/ui/components/ModTableWidget.h"
#include "app/ui/pages/RepositoryPage.h"
#include "core/config/Settings.h"
#include "core/repo/RepositoryService.h"
#include "core/repo/TagDao.h"

namespace {

constexpr int kUncategorizedCategoryId = -1;
constexpr int kUntaggedTagId = -1;

QString toDisplay(const std::string& value, const QString& fallback = {}) {
  return value.empty() ? fallback : QString::fromStdString(value);
}

QWidget* resolveParent(QWidget* preferred, QWidget* fallback) {
  return preferred ? preferred : fallback;
}

// 仅允许识别为 MOD 文件的后缀
bool isSupportedModFile(const QFileInfo& info) {
  if (!info.isFile()) {
    return false;
  }
  const QString suffix = info.suffix().toLower();
  if (suffix.isEmpty()) {
    return false;
  }
  static const std::array<QString, 4> kAllowedExt = {QStringLiteral("vpk"),
                                                     QStringLiteral("zip"),
                                                     QStringLiteral("7z"),
                                                     QStringLiteral("rar")};
  return std::any_of(kAllowedExt.begin(), kAllowedExt.end(), [&](const QString& ext) { return suffix == ext; });
}

// 规范化名称以便匹配封面
QString normalizeName(const QString& text) {
  QString normalized;
  normalized.reserve(text.size());
  for (const QChar& ch : text) {
    if (ch.isLetterOrNumber()) {
      normalized.append(ch.toLower());
    }
  }
  return normalized;
}

// 在同级目录匹配可能的封面文件
QString locateCoverCandidate(const QFileInfo& fileInfo, const QString& displayName) {
  static const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.webp"};
  const QDir dir = fileInfo.dir();
  const QString normalizedBase = normalizeName(fileInfo.completeBaseName());
  const QString normalizedDisplay = normalizeName(displayName);
  const QFileInfoList images = dir.entryInfoList(filters, QDir::Files | QDir::Readable);
  for (const QFileInfo& image : images) {
    if (!normalizedBase.isEmpty() && normalizeName(image.completeBaseName()) == normalizedBase) {
      return image.absoluteFilePath();
    }
  }
  if (!normalizedDisplay.isEmpty()) {
    for (const QFileInfo& image : images) {
      if (normalizeName(image.completeBaseName()).contains(normalizedDisplay)) {
        return image.absoluteFilePath();
      }
    }
  }
  return {};
}

// 根据文件生成初始的 ModRow 元数据
ModRow buildModFromFile(const QFileInfo& info) {
  ModRow mod;
  const QString baseName = info.completeBaseName().trimmed();
  const QString fileName = info.fileName().trimmed();
  const QString chosenName = baseName.isEmpty() ? fileName : baseName;
  mod.name = chosenName.toStdString();
  mod.file_path = QDir::toNativeSeparators(info.absoluteFilePath()).toStdString();
  mod.size_mb = static_cast<double>(info.size()) / (1024.0 * 1024.0);

  QFile file(info.absoluteFilePath());
  if (file.open(QIODevice::ReadOnly)) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
      hash.addData(file.read(1 << 16));
    }
    mod.file_hash = QString::fromLatin1(hash.result().toHex()).toStdString();
  }

  const QDateTime lastModified = info.lastModified();
  if (lastModified.isValid()) {
    const QString dateText = lastModified.date().toString(QStringLiteral("yyyy-MM-dd"));
    mod.last_published_at = dateText.toStdString();
    mod.last_saved_at = dateText.toStdString();
  }

  const QString coverPath = locateCoverCandidate(info, chosenName);
  if (!coverPath.isEmpty()) {
    mod.cover_path = QDir::toNativeSeparators(coverPath).toStdString();
  }

  const bool isNumericId =
      !baseName.isEmpty() && std::all_of(baseName.cbegin(), baseName.cend(), [](const QChar& ch) { return ch.isDigit(); });
  if (isNumericId) {
    const QString steamUrl =
        QStringLiteral("https://steamcommunity.com/sharedfiles/filedetails/?id=") + baseName;
    mod.source_url = steamUrl.toStdString();
    mod.source_platform = QStringLiteral("steam").toStdString();
  }
  return mod;
}

QString relationKindLabel(ModEditorDialog::RelationKind kind) {
  switch (kind) {
    case ModEditorDialog::RelationKind::Conflict: return RepositoryPresenter::tr("冲突");
    case ModEditorDialog::RelationKind::Requires: return RepositoryPresenter::tr("前置");
    case ModEditorDialog::RelationKind::RequiredBy: return RepositoryPresenter::tr("后置");
    case ModEditorDialog::RelationKind::Homologous: return RepositoryPresenter::tr("同质");
    case ModEditorDialog::RelationKind::CustomMaster: return RepositoryPresenter::tr("自定义（主）");
    case ModEditorDialog::RelationKind::CustomSlave: return RepositoryPresenter::tr("自定义（从）");
  }
  return RepositoryPresenter::tr("未知");
}

QString relationTargetLabel(ModEditorDialog::RelationTarget target) {
  switch (target) {
    case ModEditorDialog::RelationTarget::Mod: return RepositoryPresenter::tr("MOD");
    case ModEditorDialog::RelationTarget::Category: return RepositoryPresenter::tr("分类");
    case ModEditorDialog::RelationTarget::Tag: return RepositoryPresenter::tr("标签");
  }
  return RepositoryPresenter::tr("未知");
}

// 提取关系目标的 MOD ID，如果无法解析则返回空。
std::optional<int> resolveTargetModId(const ModEditorDialog::RelationSelection& selection,
                                      RepositoryService& repo,
                                      const QRegularExpression& idPattern) {
  if (selection.target != ModEditorDialog::RelationTarget::Mod) {
    return std::nullopt;
  }
  if (selection.targetId && *selection.targetId > 0) {
    if (repo.findMod(*selection.targetId)) {
      return selection.targetId;
    }
  }
  bool ok = false;
  int parsed = selection.targetValue.toInt(&ok);
  if (ok && parsed > 0 && repo.findMod(parsed)) {
    return parsed;
  }
  const auto match = idPattern.match(selection.targetValue);
  if (match.hasMatch()) {
    bool okId = false;
    parsed = match.captured(1).toInt(&okId);
    if (okId && parsed > 0 && repo.findMod(parsed)) {
      return parsed;
    }
  }
  return std::nullopt;
}

// 将对话框中的关系选择转换为可写入数据库的记录，同时返回告警信息。
std::vector<ModRelationRow> buildRelationRowsForMod(int modId,
                                                    const std::vector<ModEditorDialog::RelationSelection>& selections,
                                                    RepositoryService& repo,
                                                    QStringList& warnings) {
  static const QRegularExpression idPattern(QStringLiteral("(\\d+)"));
  std::set<std::tuple<QString, int, int>> dedup;
  std::vector<ModRelationRow> rows;
  rows.reserve(selections.size());

  for (const auto& selection : selections) {
    const QString kindName = relationKindLabel(selection.kind);
    if (selection.target != ModEditorDialog::RelationTarget::Mod) {
      warnings << RepositoryPresenter::tr("关系“%1”暂不支持保存到“%2”，已忽略。")
                      .arg(kindName, relationTargetLabel(selection.target));
      continue;
    }

    const auto targetIdOpt = resolveTargetModId(selection, repo, idPattern);
    if (!targetIdOpt.has_value()) {
      warnings << RepositoryPresenter::tr("关系“%1”的目标“%2”无法识别，已忽略。")
                      .arg(kindName, selection.targetValue);
      continue;
    }
    const int targetId = *targetIdOpt;
    if (targetId == modId) {
      warnings << RepositoryPresenter::tr("忽略指向自身的关系“%1”。").arg(kindName);
      continue;
    }

    QString typeName;
    int aId = 0;
    int bId = 0;
    switch (selection.kind) {
      case ModEditorDialog::RelationKind::Conflict:
        typeName = QStringLiteral("conflicts");
        aId = std::min(modId, targetId);
        bId = std::max(modId, targetId);
        break;
      case ModEditorDialog::RelationKind::Requires:
        typeName = QStringLiteral("requires");
        aId = modId;
        bId = targetId;
        break;
      case ModEditorDialog::RelationKind::RequiredBy:
        typeName = QStringLiteral("requires");
        aId = targetId;
        bId = modId;
        break;
      case ModEditorDialog::RelationKind::Homologous:
        typeName = QStringLiteral("homologous");
        aId = std::min(modId, targetId);
        bId = std::max(modId, targetId);
        break;
      case ModEditorDialog::RelationKind::CustomMaster:
        typeName = QStringLiteral("custom_master");
        aId = targetId;
        bId = modId;
        break;
      case ModEditorDialog::RelationKind::CustomSlave:
        typeName = QStringLiteral("custom_master");
        aId = modId;
        bId = targetId;
        break;
    }

    if (typeName.isEmpty() || aId == 0 || bId == 0) {
      warnings << RepositoryPresenter::tr("关系“%1”数据不完整，已忽略。").arg(kindName);
      continue;
    }

    if (selection.kind == ModEditorDialog::RelationKind::Conflict ||
        selection.kind == ModEditorDialog::RelationKind::Homologous) {
      if (aId > bId) {
        std::swap(aId, bId);
      }
    }

    const auto key = std::make_tuple(typeName, aId, bId);
    if (!dedup.insert(key).second) {
      warnings << RepositoryPresenter::tr("关系“%1”与目标 ID %2 重复，已自动跳过。")
                      .arg(kindName)
                      .arg(targetId);
      continue;
    }

    ModRelationRow row{};
    row.id = 0;
    row.a_mod_id = aId;
    row.b_mod_id = bId;
    row.type = typeName.toStdString();
    if (!selection.slotKey.isEmpty() && typeName == QStringLiteral("custom_master")) {
      row.slot_key = selection.slotKey.trimmed().toStdString();
    } else {
      row.slot_key.reset();
    }
    row.note.reset();
    rows.push_back(std::move(row));
  }

  return rows;
}

} // namespace

RepositoryPresenter::RepositoryPresenter(RepositoryPage* page,
                                         Settings& settings,
                                         QWidget* dialogParent,
                                         QObject* parent)
    : QObject(parent),
      page_(page),
      settings_(&settings),
      dialogParent_(dialogParent) {
  if (page_) {
    filterPanel_ = page_->filterPanel();
    if (filterPanel_) {
      filterAttribute_ = filterPanel_->attributeCombo();
      filterValue_ = filterPanel_->valueCombo();
    }
    showDeletedCheckBox_ = page_->showDeletedCheckBox();
    modTable_ = page_->modTable();
    coverLabel_ = page_->coverLabel();
    metaLabel_ = page_->metaLabel();
    noteView_ = page_->noteView();
  }

  filterModel_ = new QStandardItemModel(this);
  filterProxy_ = new QSortFilterProxyModel(this);
  filterProxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
  filterProxy_->setFilterKeyColumn(0);
  if (filterPanel_) {
    filterPanel_->setValueModels(filterModel_, filterProxy_);
  }

  if (page_) {
    connect(page_, &RepositoryPage::filterAttributeChanged, this, &RepositoryPresenter::handleFilterAttributeChanged);
    connect(page_, &RepositoryPage::filterValueChanged, this, &RepositoryPresenter::handleFilterChanged);
    connect(page_, &RepositoryPage::filterValueTextChanged, this, &RepositoryPresenter::handleFilterValueTextChanged);
    connect(page_, &RepositoryPage::importRequested, this, &RepositoryPresenter::handleImportRequested);
    // 绑定批量导入按钮的响应
    connect(page_, &RepositoryPage::importFolderRequested, this, &RepositoryPresenter::handleImportFolderRequested);
    connect(page_, &RepositoryPage::editRequested, this, &RepositoryPresenter::handleEditRequested);
    connect(page_, &RepositoryPage::deleteRequested, this, &RepositoryPresenter::handleDeleteRequested);
    connect(page_, &RepositoryPage::refreshRequested, this, &RepositoryPresenter::handleRefreshRequested);
    connect(page_, &RepositoryPage::showDeletedToggled, this, &RepositoryPresenter::handleShowDeletedToggled);
    connect(page_, &RepositoryPage::currentCellChanged, this, &RepositoryPresenter::handleCurrentCellChanged);
  }
}

void RepositoryPresenter::setRepositoryService(RepositoryService* repo) {
  repo_ = repo;
}

void RepositoryPresenter::setImportService(ImportService* service) {
  importService_ = service;
}

void RepositoryPresenter::setRepositoryDirectory(const QString& path) {
  repoDir_ = path;
}

void RepositoryPresenter::initializeFilters() {
  if (filterAttribute_) {
    filterAttribute_->clear();
    filterAttribute_->addItems({tr("名称"), tr("分类"), tr("标签"), tr("作者"), tr("评分")});
    filterAttribute_->setCurrentText(tr("名称"));
  }
  if (filterValue_) {
    filterValue_->setEnabled(true);
    if (auto* edit = filterValue_->lineEdit()) {
      edit->setClearButtonEnabled(true);
      edit->setPlaceholderText(tr("搜索名称"));
    }
  }
}

void RepositoryPresenter::reloadAll() {
  reloadCategories();
  loadData();
  if (filterAttribute_) {
    handleFilterAttributeChanged(filterAttribute_->currentText());
  }
}

QString RepositoryPresenter::tagsTextForMod(int modId) const {
  const auto it = modTagsText_.find(modId);
  return it != modTagsText_.end() ? it->second : QString();
}

std::vector<TagDescriptor> RepositoryPresenter::tagsForMod(int modId) const {
  std::vector<TagDescriptor> tags;
  if (!repo_) {
    return tags;
  }
  const auto rows = repo_->listTagsForMod(modId);
  tags.reserve(rows.size());
  for (const auto& row : rows) {
    tags.push_back({row.group_name, row.name});
  }
  return tags;
}

QString RepositoryPresenter::categoryNameFor(int categoryId) const {
  if (categoryId > 0) {
    const auto it = categoryNames_.find(categoryId);
    if (it != categoryNames_.end()) {
      return it->second;
    }
    return QStringLiteral("Category#%1").arg(categoryId);
  }
  return tr("未分类");
}

bool RepositoryPresenter::modMatchesFilter(const ModRow& mod,
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

void RepositoryPresenter::populateCategoryFilterModel(QStandardItemModel* model, bool updateCache) {
  if (!repo_) {
    return;
  }

  if (updateCache) {
    categoryNames_.clear();
    categoryParent_.clear();
  }

  if (model) {
    model->clear();
    auto* uncategorizedItem = new QStandardItem(tr("未分类"));
    uncategorizedItem->setData(kUncategorizedCategoryId, Qt::UserRole);
    model->appendRow(uncategorizedItem);
  }

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
    std::sort(entry.second.begin(), entry.second.end(), compare);
  }

  if (!model) {
    return;
  }

  for (const auto& parent : topLevel) {
    auto* parentItem = new QStandardItem(QString::fromStdString(parent.name));
    parentItem->setData(parent.id, Qt::UserRole);
    model->appendRow(parentItem);

    const auto childIt = children.find(parent.id);
    if (childIt == children.end()) continue;
    for (const auto& child : childIt->second) {
      auto* childItem = new QStandardItem(QStringLiteral("  ") + QString::fromStdString(child.name));
      childItem->setData(child.id, Qt::UserRole);
      model->appendRow(childItem);
    }
  }
}

void RepositoryPresenter::populateTagFilterModel(QStandardItemModel* model) const {
  if (!model || !repo_) {
    return;
  }

  model->clear();
  auto* untaggedItem = new QStandardItem(tr("无标签"));
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

  std::vector<GroupBucket> buckets;
  buckets.reserve(groupedTags.size());
  for (auto& entry : groupedTags) {
    auto& bucket = entry.second;
    std::sort(bucket.tags.begin(), bucket.tags.end(), [](const TagWithGroupRow& a, const TagWithGroupRow& b) {
      if (a.priority != b.priority) return a.priority < b.priority;
      return a.name < b.name;
    });
    buckets.push_back(std::move(bucket));
  }
  std::sort(buckets.begin(), buckets.end(), [](const GroupBucket& a, const GroupBucket& b) {
    if (a.priority != b.priority) return a.priority < b.priority;
    return a.name < b.name;
  });

  for (const auto& bucket : buckets) {
    for (const auto& tag : bucket.tags) {
      auto* item = new QStandardItem(QStringLiteral("[%1] %2")
                                         .arg(bucket.name, QString::fromStdString(tag.name)));
      item->setData(tag.id, Qt::UserRole);
      model->appendRow(item);
    }
  }
}

void RepositoryPresenter::populateAuthorFilterModel(QStandardItemModel* model) const {
  if (!model) {
    return;
  }

  model->clear();

  QStringList authors;
  authors.reserve(static_cast<int>(mods_.size()));
  for (const auto& mod : mods_) {
    if (!mod.author.empty()) {
      const QString author = QString::fromStdString(mod.author);
      if (!authors.contains(author)) {
        authors.append(author);
      }
    }
  }
  authors.sort(Qt::CaseInsensitive);

  for (const auto& author : authors) {
    auto* item = new QStandardItem(author);
    item->setData(author, Qt::UserRole);
    model->appendRow(item);
  }
}

void RepositoryPresenter::populateRatingFilterModel(QStandardItemModel* model) const {
  if (!model) {
    return;
  }

  model->clear();
  auto* unratedItem = new QStandardItem(tr("未评分"));
  unratedItem->setData(0, Qt::UserRole);
  model->appendRow(unratedItem);

  for (int rating = 1; rating <= 5; ++rating) {
    auto* item = new QStandardItem(QString::number(rating));
    item->setData(rating, Qt::UserRole);
    model->appendRow(item);
  }
}

void RepositoryPresenter::updateDetailForMod(int modId) {
  if (!coverLabel_ || !metaLabel_ || !noteView_) {
    return;
  }

  if (modId <= 0) {
    coverLabel_->setPixmap(QPixmap());
    coverLabel_->setText(tr("未选择 MOD"));
    noteView_->clear();
    metaLabel_->clear();
    return;
  }

  if (!repo_) {
    return;
  }

  const auto modOpt = repo_->findMod(modId);
  if (!modOpt) {
    coverLabel_->setPixmap(QPixmap());
    coverLabel_->setText(tr("记录不存在"));
    noteView_->clear();
    metaLabel_->clear();
    return;
  }

  const auto& mod = *modOpt;
  const QString categoryName = categoryNameFor(mod.category_id);
  QString tags;
  const auto cacheIt = modTagsCache_.find(mod.id);
  if (cacheIt != modTagsCache_.end()) {
    tags = formatTagSummary(cacheIt->second, QStringLiteral("\n"), QStringLiteral(" / "));
  } else if (repo_) {
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
    coverLabel_->setText(tr("无预览"));
  }

  noteView_->setPlainText(QString::fromStdString(mod.note));

  QStringList meta;
  meta << tr("名称：%1").arg(QString::fromStdString(mod.name));
  meta << tr("分类：%1").arg(categoryName.isEmpty() ? tr("未分类") : categoryName);
  meta << tr("标签：%1").arg(tags.isEmpty() ? tr("无") : tags);
  meta << tr("作者：%1").arg(toDisplay(mod.author, tr("未知")));
  meta << tr("评分：%1").arg(mod.rating > 0 ? QString::number(mod.rating) : tr("未评分"));
  meta << tr("大小：%1 MB").arg(QString::number(mod.size_mb, 'f', 2));
  meta << tr("状态：%1").arg(toDisplay(mod.status, tr("未知")));
  meta << tr("最后发布：%1").arg(toDisplay(mod.last_published_at, tr("-")));
  meta << tr("最后保存：%1").arg(toDisplay(mod.last_saved_at, tr("-")));
  meta << tr("安全性：%1").arg(toDisplay(mod.integrity, tr("-")));
  meta << tr("稳定性：%1").arg(toDisplay(mod.stability, tr("-")));
  meta << tr("获取方式：%1").arg(toDisplay(mod.acquisition_method, tr("-")));
  if (!mod.source_platform.empty()) meta << tr("平台：%1").arg(QString::fromStdString(mod.source_platform));
  if (!mod.source_url.empty()) meta << tr("链接：%1").arg(QString::fromStdString(mod.source_url));
  if (!mod.file_path.empty()) meta << tr("文件：%1").arg(QString::fromStdString(mod.file_path));
  if (!mod.file_hash.empty()) meta << tr("校验：%1").arg(QString::fromStdString(mod.file_hash));
  metaLabel_->setText(meta.join('\n'));
}

int RepositoryPresenter::filterIdForCombo(const QComboBox* combo,
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

void RepositoryPresenter::handleRefreshRequested() {
  reloadCategories();
  loadData();
}

void RepositoryPresenter::handleImportRequested() {
  if (!repo_) {
    return;
  }

  ModEditorDialog dialog(*repo_, resolveParent(dialogParent_, page_));
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  ModRow mod = dialog.modData();
  const auto relations = dialog.relationSelections();
  QStringList transferErrors;
  if (!importService_ || !importService_->ensureModFilesInRepository(*settings_, mod, transferErrors)) {
    QMessageBox::warning(resolveParent(dialogParent_, page_), tr("导入失败"), transferErrors.join(QStringLiteral("\n")));
    return;
  }

  try {
    const int newModId = repo_->createModWithTags(mod, dialog.selectedTags());
    mod.id = newModId;
    QStringList relationWarnings;
    const auto relationRows = buildRelationRowsForMod(newModId, relations, *repo_, relationWarnings);
    repo_->replaceRelationsForMod(newModId, relationRows);
    if (!relationWarnings.isEmpty()) {
      QMessageBox::warning(resolveParent(dialogParent_, page_), tr("关系处理提示"),
                           relationWarnings.join(QStringLiteral("\n")));
    }
    loadData();
  } catch (const std::exception& e) {
    QMessageBox::warning(resolveParent(dialogParent_, page_), tr("导入失败"),
                         tr("MOD 入库失败：%1").arg(QString::fromUtf8(e.what())));
  }
}

void RepositoryPresenter::handleImportFolderRequested() {
  if (!repo_) {
    return;
  }

  ImportFolderDialog dialog(resolveParent(dialogParent_, page_));
  // 默认使用仓库目录作为初始路径
  QString initialDir = repoDir_;
  if (initialDir.isEmpty() && settings_) {
    initialDir = QString::fromStdString(settings_->repoDir);
  }
  if (!initialDir.isEmpty()) {
    dialog.setDirectory(initialDir);
  }

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const QString targetDir = dialog.directory();
  const bool recursive = dialog.includeSubdirectories();
  QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
  QDirIterator iterator(targetDir, QStringList(), QDir::Files | QDir::Readable, flags);

  QStringList filePaths;
  while (iterator.hasNext()) {
    const QString path = iterator.next();
    const QFileInfo info(path);
    if (isSupportedModFile(info)) {
      filePaths.append(path);
    }
  }

  if (filePaths.isEmpty()) {
    QMessageBox::information(resolveParent(dialogParent_, page_), tr("未发现 MOD"),
                             tr("所选文件夹中没有符合条件的 MOD 文件（vpk/zip/7z/rar）"));
    return;
  }

  if (!importService_ || !settings_) {
    QMessageBox::warning(resolveParent(dialogParent_, page_), tr("导入失败"), tr("导入服务未初始化，无法执行批量导入"));
    return;
  }

  int successCount = 0;
  QStringList failureMessages;

  for (const QString& path : filePaths) {
    const QFileInfo info(path);
    ModRow mod = buildModFromFile(info);
    QStringList transferErrors;
    // 逐项执行文件搬运
    if (!importService_->ensureModFilesInRepository(*settings_, mod, transferErrors)) {
      const QString detail = transferErrors.join(QStringLiteral("；"));
      failureMessages << tr("%1：%2").arg(info.fileName()).arg(detail.isEmpty() ? tr("文件转移失败") : detail);
      continue;
    }
    try {
      repo_->createModWithTags(mod, {});
      ++successCount;
    } catch (const std::exception& e) {
      failureMessages << tr("%1：%2").arg(info.fileName()).arg(QString::fromUtf8(e.what()));
    }
  }

  if (successCount > 0) {
    loadData();
  }

  QString summary = tr("成功导入 %1 个 MOD").arg(successCount);
  if (failureMessages.isEmpty()) {
    QMessageBox::information(resolveParent(dialogParent_, page_), tr("批量导入完成"), summary);
  } else {
    summary.append(tr("\n失败 %1 个：\n%2").arg(failureMessages.size()).arg(failureMessages.join(QStringLiteral("\n"))));
    QMessageBox::warning(resolveParent(dialogParent_, page_), tr("部分导入失败"), summary);
  }
}

void RepositoryPresenter::handleEditRequested() {
  if (!repo_ || !modTable_) {
    return;
  }

  auto* currentItem = modTable_->currentItem();
  if (!currentItem) {
    QMessageBox::information(resolveParent(dialogParent_, page_), tr("未选择"), tr("请先选择一个 MOD。"));
    return;
  }

  const int modId = modTable_->item(modTable_->currentRow(), 0)->data(Qt::UserRole).toInt();
  auto modOpt = repo_->findMod(modId);
  if (!modOpt) {
    QMessageBox::warning(resolveParent(dialogParent_, page_), tr("缺失"), tr("该 MOD 记录已不存在。"));
    loadData();
    return;
  }

  ModEditorDialog dialog(*repo_, resolveParent(dialogParent_, page_));
  dialog.setMod(*modOpt, tagsForMod(modId));
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  ModRow updated = dialog.modData();
  const auto relations = dialog.relationSelections();
  QStringList transferErrors;
  if (!importService_ || !importService_->ensureModFilesInRepository(*settings_, updated, transferErrors)) {
    QMessageBox::warning(resolveParent(dialogParent_, page_), tr("更新失败"), transferErrors.join(QStringLiteral("\n")));
    return;
  }

  try {
    repo_->updateModWithTags(updated, dialog.selectedTags());
    QStringList relationWarnings;
    const auto relationRows = buildRelationRowsForMod(updated.id, relations, *repo_, relationWarnings);
    repo_->replaceRelationsForMod(updated.id, relationRows);
    if (!relationWarnings.isEmpty()) {
      QMessageBox::warning(resolveParent(dialogParent_, page_), tr("关系处理提示"),
                           relationWarnings.join(QStringLiteral("\n")));
    }
    loadData();
    updateDetailForMod(updated.id);
  } catch (const std::exception& e) {
    QMessageBox::warning(resolveParent(dialogParent_, page_), tr("更新失败"),
                         tr("MOD 更新失败：%1").arg(QString::fromUtf8(e.what())));
  }
}

void RepositoryPresenter::handleDeleteRequested() {
  if (!repo_ || !modTable_) {
    return;
  }

  auto* item = modTable_->currentItem();
  if (!item) {
    return;
  }
  const int modId = modTable_->item(modTable_->currentRow(), 0)->data(Qt::UserRole).toInt();

  const auto reply = QMessageBox::question(resolveParent(dialogParent_, page_), tr("删除 MOD"),
                                           tr("是否仅标记为已删除？"), QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    return;
  }

  repo_->setModDeleted(modId, true);
  loadData();
}

void RepositoryPresenter::handleShowDeletedToggled(bool /*checked*/) {
  populateTable();
}

void RepositoryPresenter::handleFilterAttributeChanged(const QString& attribute) {
  if (!filterValue_ || !filterModel_ || !filterProxy_) {
    return;
  }

  suppressFilterSignals_ = true;
  filterValue_->blockSignals(true);

  filterModel_->clear();
  filterProxy_->setFilterFixedString(QString());

  QString placeholder;

  if (attribute == tr("名称")) {
    filterValue_->setEnabled(true);
    placeholder = tr("搜索名称");
  } else if (attribute == tr("分类")) {
    filterValue_->setEnabled(true);
    reloadCategories();
    placeholder = tr("选择分类");
  } else if (attribute == tr("标签")) {
    filterValue_->setEnabled(true);
    reloadTags();
    placeholder = tr("选择标签");
  } else if (attribute == tr("作者")) {
    filterValue_->setEnabled(true);
    reloadAuthors();
    placeholder = tr("选择作者");
  } else if (attribute == tr("评分")) {
    filterValue_->setEnabled(true);
    reloadRatings();
    placeholder = tr("选择评分");
  } else {
    filterValue_->setEnabled(false);
  }

  filterValue_->setModel(nullptr);
  filterValue_->setModel(filterProxy_);
  filterValue_->setEditText(QString());
  filterValue_->setCurrentIndex(-1);

  if (auto* lineEdit = filterValue_->lineEdit()) {
    lineEdit->setPlaceholderText(placeholder);
  }

  filterValue_->blockSignals(false);
  suppressFilterSignals_ = false;

  populateTable();
}

void RepositoryPresenter::handleFilterValueTextChanged(const QString& text) {
  if (!filterProxy_ || suppressFilterSignals_) {
    return;
  }
  filterProxy_->setFilterFixedString(text);

  const QString currentFilter = filterAttribute_ ? filterAttribute_->currentText() : QString();
  if ((currentFilter == tr("名称") || currentFilter == tr("标签")) && filterValue_ && filterValue_->lineEdit()) {
    if (filterValue_->lineEdit()->hasFocus()) {
      filterValue_->showPopup();
    }
  }
}

void RepositoryPresenter::handleFilterChanged(const QString& /*text*/) {
  if (!suppressFilterSignals_) {
    populateTable();
  }
}

void RepositoryPresenter::handleCurrentCellChanged(int currentRow,
                                                   int currentColumn,
                                                   int previousRow,
                                                   int previousColumn) {
  Q_UNUSED(currentColumn);
  Q_UNUSED(previousRow);
  Q_UNUSED(previousColumn);

  if (!modTable_) {
    return;
  }
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

void RepositoryPresenter::loadData() {
  if (!repo_) {
    return;
  }

  mods_ = repo_->listAll(true);
  modTagsText_.clear();
  modTagsCache_.clear();
  for (const auto& mod : mods_) {
    auto tagRows = repo_->listTagsForMod(mod.id);
    modTagsCache_[mod.id] = tagRows;
    modTagsText_[mod.id] = formatTagSummary(tagRows, QStringLiteral("  |  "), QStringLiteral(" / "));
  }

  populateTable();
  emit modsReloaded();
}

void RepositoryPresenter::populateTable() {
  if (!modTable_ || !filterAttribute_ || !filterValue_) {
    return;
  }

  modTable_->setRowCount(0);

  const QString filterAttribute = filterAttribute_->currentText();
  const QString filterValueText = filterValue_->currentText();
  const int filterId = filterIdForCombo(filterValue_, filterProxy_, filterModel_);

  int row = 0;
  for (const auto& mod : mods_) {
    if (showDeletedCheckBox_ && !showDeletedCheckBox_->isChecked() && mod.is_deleted) {
      continue;
    }

    if (!modMatchesFilter(mod, filterAttribute, filterId, filterValueText)) {
      continue;
    }

    const QString name = QString::fromStdString(mod.name);
    const QString author = toDisplay(mod.author);
    const QString status = toDisplay(mod.status, tr("未知"));
    const QString lastPublished = toDisplay(mod.last_published_at, tr("-"));
    const QString lastSaved = toDisplay(mod.last_saved_at, tr("-"));
    const QString platform = toDisplay(mod.source_platform);
    const QString url = toDisplay(mod.source_url);
    const QString note = toDisplay(mod.note);
    const QString integrity = toDisplay(mod.integrity);
    const QString stability = toDisplay(mod.stability);
    const QString acquisition = toDisplay(mod.acquisition_method);
    const QString tags = tagsTextForMod(mod.id);

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

void RepositoryPresenter::reloadCategories() {
  const bool usingCategoryFilter = filterAttribute_ && filterAttribute_->currentText() == tr("分类");
  if (!usingCategoryFilter && filterModel_) {
    filterModel_->clear();
  }
  populateCategoryFilterModel(usingCategoryFilter ? filterModel_ : nullptr, true);
}

void RepositoryPresenter::reloadTags() {
  populateTagFilterModel(filterModel_);
}

void RepositoryPresenter::reloadAuthors() {
  populateAuthorFilterModel(filterModel_);
}

void RepositoryPresenter::reloadRatings() {
  populateRatingFilterModel(filterModel_);
}

QString RepositoryPresenter::formatTagSummary(const std::vector<TagWithGroupRow>& rows,
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

bool RepositoryPresenter::categoryMatchesFilter(int modCategoryId, int filterCategoryId) const {
  if (filterCategoryId == kUncategorizedCategoryId) {
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
