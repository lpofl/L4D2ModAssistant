#include "app/services/GameDirectoryMonitor.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <spdlog/spdlog.h>

#include "app/services/ImportService.h"
#include "core/repo/GameModDao.h"

#include <cmath>
#include <utility>

namespace {

// 支持识别的 MOD 文件扩展名集合。
const QStringList kModExtensions = {QStringLiteral("vpk"),
                                    QStringLiteral("zip"),
                                    QStringLiteral("7z"),
                                    QStringLiteral("rar")};

inline QString cleanPath(const std::string& path) {
  return QDir::cleanPath(QDir::fromNativeSeparators(QString::fromStdString(path)));
}

inline double bytesToMb(std::uint64_t bytes) {
  return static_cast<double>(bytes) / (1024.0 * 1024.0);
}

inline std::uint64_t mbToBytes(double mb) {
  return static_cast<std::uint64_t>(std::llround(mb * 1024.0 * 1024.0));
}

}  // namespace

GameDirectoryMonitor::GameDirectoryMonitor(QObject* parent)
    : QObject(parent) {
  connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, &GameDirectoryMonitor::onDirectoryChanged);
  connect(&watcher_, &QFileSystemWatcher::fileChanged, this, &GameDirectoryMonitor::onFileChanged);
}

void GameDirectoryMonitor::configure(const Settings& settings,
                                     RepositoryService* repoService,
                                     ImportService* importService) {
  settings_ = settings;
  repoService_ = repoService;
  importService_ = importService;
  addonsDir_ = cleanPath(settings.addonsPath);
  workshopDir_ = cleanPath(settings.workshopPath);

  QStringList directories;
  if (!addonsDir_.isEmpty()) {
    directories << addonsDir_;
  }
  if (!workshopDir_.isEmpty()) {
    directories << workshopDir_;
  }
  updateDirectoryWatches(directories);
  initialScanCompleted_ = false;
  rescanAll();
}

void GameDirectoryMonitor::onDirectoryChanged(const QString& path) {
  Q_UNUSED(path);
  rescanAll();
}

void GameDirectoryMonitor::onFileChanged(const QString& path) {
  Q_UNUSED(path);
  rescanAll();
}

void GameDirectoryMonitor::rescanAll() {
  if (!repoService_) {
    return;
  }
  const bool isInitial = !initialScanCompleted_;
  RepoInventory inventory = buildInventory();
  QSet<QString> files;
  QStringList updatedMods;

  if (!addonsDir_.isEmpty()) {
    rescanSource(QStringLiteral("addons"), addonsDir_, inventory, files, updatedMods);
  } else {
    repoService_->replaceGameModsForSource("addons", {});
  }

  if (!workshopDir_.isEmpty()) {
    rescanSource(QStringLiteral("workshop"), workshopDir_, inventory, files, updatedMods);
  } else {
    repoService_->replaceGameModsForSource("workshop", {});
  }

  updateFileWatches(files);
  emit gameModsUpdated(updatedMods, isInitial);
  if (isInitial) {
    initialScanCompleted_ = true;
  }
}

void GameDirectoryMonitor::rescanSource(const QString& sourceKey,
                                        const QString& directory,
                                        RepoInventory& inventory,
                                        QSet<QString>& watchedFiles,
                                        QStringList& updatedMods) {
  if (!repoService_) {
    return;
  }

  QDir dir(directory);
  if (!dir.exists()) {
    repoService_->replaceGameModsForSource(sourceKey.toStdString(), {});
    return;
  }

  dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::Readable);
  QFileInfoList entries = dir.entryInfoList();
  std::vector<GameModRow> rows;
  rows.reserve(entries.size());

  const auto nowIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
  const bool isWorkshop = (sourceKey == QStringLiteral("workshop"));
  const QRegularExpression numericPattern(QStringLiteral("^\\d+$"));

  for (const QFileInfo& info : entries) {
    if (!info.isFile()) {
      continue;
    }
    const QString suffix = info.suffix().toLower();
    if (!kModExtensions.contains(suffix)) {
      continue;
    }

    watchedFiles.insert(info.absoluteFilePath());

    const QString normalizedName = normalizeKey(info.completeBaseName());
    const std::uint64_t sizeBytes = static_cast<std::uint64_t>(info.size());
    int matchedIndex = -1;
    ModRow* matchedMod = nullptr;

    if (sourceKey == QStringLiteral("addons")) {
      const ModRow* mod = findAddonMatch(normalizedName, sizeBytes, inventory, matchedIndex);
      matchedMod = mod ? &inventory.mods[matchedIndex] : nullptr;
    } else {
      QString numericId;
      if (numericPattern.match(info.completeBaseName()).hasMatch()) {
        numericId = info.completeBaseName();
      }
      const ModRow* mod = findWorkshopMatch(normalizedName, numericId, inventory, matchedIndex);
      matchedMod = mod ? &inventory.mods[matchedIndex] : nullptr;
      if (matchedMod) {
        if (auto updatedName = synchronizeWorkshopIfNeeded(info, inventory.mods[matchedIndex], numericId)) {
          updatedMods.append(*updatedName);
        }
      }
    }

    GameModRow row;
    row.name = info.fileName().toStdString();
    row.file_path = QDir::toNativeSeparators(info.absoluteFilePath()).toStdString();
    row.source = sourceKey.toStdString();
    row.file_size = sizeBytes;
    row.modified_at = info.lastModified().toUTC().toString(Qt::ISODateWithMs).toStdString();
    row.last_scanned_at = nowIso.toStdString();

    if (matchedMod) {
      row.repo_mod_id = matchedMod->id;
      row.status = resolveStatus(matchedMod, sizeBytes, sourceKey).toStdString();
    } else {
      row.repo_mod_id.reset();
      row.status = tr("未入库").toStdString();
    }

    rows.push_back(std::move(row));
  }

  repoService_->replaceGameModsForSource(sourceKey.toStdString(), rows);
}

GameDirectoryMonitor::RepoInventory GameDirectoryMonitor::buildInventory() const {
  RepoInventory inventory;
  if (!repoService_) {
    return inventory;
  }
  inventory.mods = repoService_->listAll(true);
  inventory.nameIndex.clear();
  inventory.steamIdIndex.clear();

  for (size_t i = 0; i < inventory.mods.size(); ++i) {
    const ModRow& mod = inventory.mods[i];
    const QString normalized = normalizeKey(QString::fromStdString(mod.name));
    if (!normalized.isEmpty()) {
      inventory.nameIndex.emplace(normalized.toStdString(), static_cast<int>(i));
    }
    const QString workshopId = extractWorkshopId(mod.source_url);
    if (!workshopId.isEmpty()) {
      inventory.steamIdIndex.emplace(workshopId.toStdString(), static_cast<int>(i));
    }
  }
  return inventory;
}

QString GameDirectoryMonitor::normalizeKey(const QString& text) const {
  QString normalized;
  normalized.reserve(text.size());
  for (const QChar& ch : text) {
    if (ch.isLetterOrNumber()) {
      normalized.append(ch.toLower());
    }
  }
  return normalized.trimmed();
}

QString GameDirectoryMonitor::extractWorkshopId(const std::string& url) const {
  if (url.empty()) {
    return {};
  }
  static const QRegularExpression pattern(QStringLiteral("id=(\\d+)"));
  const QString qurl = QString::fromStdString(url);
  const auto match = pattern.match(qurl);
  return match.hasMatch() ? match.captured(1) : QString();
}

const ModRow* GameDirectoryMonitor::findAddonMatch(const QString& normalizedName,
                                                   std::uint64_t fileSize,
                                                   RepoInventory& inventory,
                                                   int& matchedIndex) const {
  if (normalizedName.isEmpty()) {
    return nullptr;
  }
  auto range = inventory.nameIndex.equal_range(normalizedName.toStdString());
  for (auto it = range.first; it != range.second; ++it) {
    const int index = it->second;
    const ModRow& candidate = inventory.mods[index];
    if (mbToBytes(candidate.size_mb) == fileSize) {
      matchedIndex = index;
      return &candidate;
    }
  }
  return nullptr;
}

const ModRow* GameDirectoryMonitor::findWorkshopMatch(const QString& normalizedName,
                                                      const QString& numericId,
                                                      RepoInventory& inventory,
                                                      int& matchedIndex) const {
  if (!normalizedName.isEmpty()) {
    auto range = inventory.nameIndex.equal_range(normalizedName.toStdString());
    if (range.first != range.second) {
      matchedIndex = range.first->second;
      return &inventory.mods[matchedIndex];
    }
  }
  if (!numericId.isEmpty()) {
    auto range = inventory.steamIdIndex.equal_range(numericId.toStdString());
    if (range.first != range.second) {
      matchedIndex = range.first->second;
      return &inventory.mods[matchedIndex];
    }
  }
  return nullptr;
}

QString GameDirectoryMonitor::resolveStatus(const ModRow* mod,
                                            std::uint64_t fileSizeBytes,
                                            const QString& sourceKey) const {
  if (!mod) {
    return tr("未入库");
  }
  const QString repoFile = cleanPath(mod->file_path);
  const bool repoExists = !repoFile.isEmpty() && QFileInfo::exists(repoFile);
  if (mod->is_deleted || !repoExists) {
    return tr("仓库无vpk文件");
  }
  if (sourceKey == QStringLiteral("addons")) {
    if (mbToBytes(mod->size_mb) != fileSizeBytes) {
      return tr("未入库");
    }
  }
  return tr("已入库");
}

std::optional<QString> GameDirectoryMonitor::synchronizeWorkshopIfNeeded(const QFileInfo& fileInfo,
                                                                         ModRow& modRecord,
                                                                         const QString& numericId) {
  Q_UNUSED(numericId);
  if (!repoService_) {
    return std::nullopt;
  }

  const QDateTime workshopMtime = fileInfo.lastModified();
  QDateTime repoMtime;
  const QString recordedRepoPath = cleanPath(modRecord.file_path);
  if (!recordedRepoPath.isEmpty()) {
    const QFileInfo repoInfo(recordedRepoPath);
    if (repoInfo.exists()) {
      repoMtime = repoInfo.lastModified();
    }
  }
  if (!repoMtime.isValid() && !modRecord.last_saved_at.empty()) {
    const QDate date = QDate::fromString(QString::fromStdString(modRecord.last_saved_at), QStringLiteral("yyyy-MM-dd"));
    if (date.isValid()) {
      repoMtime = QDateTime(date, QTime(0, 0));
    }
  }
  if (repoMtime.isValid() && repoMtime >= workshopMtime) {
    return std::nullopt; // 仓库记录不旧于 workshop，无需同步
  }

  const QString repoRoot = cleanPath(settings_.repoDir);
  if (repoRoot.isEmpty()) {
    spdlog::warn("Repository directory not configured, skip workshop sync for {}", modRecord.name);
    return std::nullopt;
  }

  const QString sourcePath = fileInfo.absoluteFilePath();
  QString targetPath = recordedRepoPath;
  auto ensureDirectory = [](const QString& path) {
    const QFileInfo info(path);
    QDir dir = info.dir();
    if (!dir.exists()) {
      dir.mkpath(QStringLiteral("."));
    }
  };

  const auto allocateTarget = [&](const QString& baseDir, const QString& fileName) {
    // 始终覆盖同名文件，避免重复生成带后缀的副本
    QDir base(baseDir);
    return QDir::cleanPath(base.filePath(fileName));
  };

  if (targetPath.isEmpty()) {
    targetPath = allocateTarget(repoRoot, fileInfo.fileName());
  }
  ensureDirectory(targetPath);
  if (!copyReplacing(sourcePath, targetPath)) {
    spdlog::warn("Failed to copy workshop file {} -> {}", sourcePath.toStdString(), targetPath.toStdString());
    return std::nullopt;
  }

  QString coverSource = locateWorkshopCover(fileInfo);
  QString coverTarget = cleanPath(modRecord.cover_path);
  if (!coverSource.isEmpty()) {
    if (coverTarget.isEmpty()) {
      coverTarget = allocateTarget(QFileInfo(targetPath).dir().absolutePath(), QFileInfo(coverSource).fileName());
    }
    ensureDirectory(coverTarget);
    if (copyReplacing(coverSource, coverTarget)) {
      modRecord.cover_path = QDir::toNativeSeparators(coverTarget).toStdString();
    }
  }

  QFile repoFile(targetPath);
  if (!repoFile.open(QIODevice::ReadOnly)) {
    spdlog::warn("Failed to open copied workshop file for hashing: {}", targetPath.toStdString());
    return std::nullopt;
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!repoFile.atEnd()) {
    hash.addData(repoFile.read(1 << 20));
  }

  modRecord.file_hash = QString::fromLatin1(hash.result().toHex()).toStdString();
  modRecord.file_path = QDir::toNativeSeparators(targetPath).toStdString();
  modRecord.size_mb = bytesToMb(static_cast<std::uint64_t>(fileInfo.size()));
  const QString dateText = workshopMtime.date().toString(QStringLiteral("yyyy-MM-dd"));
  modRecord.last_saved_at = dateText.toStdString();
  modRecord.last_published_at = dateText.toStdString();

  auto tagDescriptors = tagsForMod(modRecord.id);
  try {
    repoService_->updateModWithTags(modRecord, tagDescriptors);
    spdlog::info("Workshop mod {} synchronized to repository.", modRecord.name);
    return QString::fromStdString(modRecord.name);
  } catch (const std::exception& ex) {
    spdlog::error("Failed to update repository record for {}: {}", modRecord.name, ex.what());
    return std::nullopt;
  }
}

QString GameDirectoryMonitor::locateWorkshopCover(const QFileInfo& fileInfo) const {
  static const QStringList kImageExt = {"png", "jpg", "jpeg", "bmp", "webp"};
  const QString base = fileInfo.completeBaseName();
  if (base.isEmpty()) {
    return {};
  }
  const QDir dir = fileInfo.dir();
  for (const QString& ext : kImageExt) {
    const QString candidate = dir.filePath(QStringLiteral("%1.%2").arg(base, ext));
    if (QFileInfo::exists(candidate)) {
      return candidate;
    }
  }
  return {};
}

bool GameDirectoryMonitor::copyReplacing(const QString& src, const QString& dst) const {
  if (src == dst) {
    return true;
  }
  QFileInfo dstInfo(dst);
  QDir dir = dstInfo.dir();
  if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
    return false;
  }
  if (QFile::exists(dst) && !QFile::remove(dst)) {
    return false;
  }
  return QFile::copy(src, dst);
}

std::vector<TagDescriptor> GameDirectoryMonitor::tagsForMod(int modId) const {
  std::vector<TagDescriptor> tags;
  if (!repoService_) {
    return tags;
  }
  auto rows = repoService_->listTagsForMod(modId);
  tags.reserve(rows.size());
  for (const auto& row : rows) {
    TagDescriptor descriptor;
    descriptor.group = row.group_name;
    descriptor.tag = row.name;
    tags.push_back(std::move(descriptor));
  }
  return tags;
}

void GameDirectoryMonitor::updateDirectoryWatches(const QStringList& directories) {
  for (const QString& dir : watchedDirectories_) {
    watcher_.removePath(dir);
  }
  watchedDirectories_ = directories;
  for (const QString& dir : directories) {
    if (!dir.isEmpty()) {
      watcher_.addPath(dir);
    }
  }
}

void GameDirectoryMonitor::updateFileWatches(const QSet<QString>& newFiles) {
  for (const QString& path : std::as_const(watchedFiles_)) {
    if (!newFiles.contains(path)) {
      watcher_.removePath(path);
    }
  }
  for (const QString& path : newFiles) {
    if (!watchedFiles_.contains(path)) {
      watcher_.addPath(path);
    }
  }
  watchedFiles_ = newFiles;
}
