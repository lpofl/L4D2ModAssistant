#pragma once

#include <QFileSystemWatcher>
#include <QFileInfo>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "core/config/Settings.h"
#include "core/repo/RepositoryService.h"

class ImportService;

/**
 * @brief 负责扫描并监听游戏目录下 addons/workshop 的 MOD 变化。
 *
 * - 首次配置时立即进行一次全量扫描，仅采集基础元数据并写入 gamemods 缓存表。
 * - 后续通过 QFileSystemWatcher 监听目录/文件变动，触发增量刷新。
 * - 同时负责检测 workshop 中的 MOD 是否较仓库版本更新，如有则执行文件同步与仓库记录更新。
 */
class GameDirectoryMonitor : public QObject {
  Q_OBJECT
public:
  explicit GameDirectoryMonitor(QObject* parent = nullptr);

  /**
   * @brief 配置扫描所需的依赖并开始监听。
   * @param settings 最新的应用设置（包含游戏与仓库目录）
   * @param repoService 仓库服务指针
   * @param importService 导入服务指针，用于重用文件搬运逻辑
   */
  void configure(const Settings& settings,
                 RepositoryService* repoService,
                 ImportService* importService);

signals:
  /// 缓存内容更新后发射，提示 UI 重新加载游戏目录列表。
  /// @param updatedMods 本次扫描期间同步到仓库的 MOD 名称列表。
  /// @param initialScan true 表示这是应用启动后的首次全量扫描。
  void gameModsUpdated(const QStringList& updatedMods, bool initialScan);

private slots:
  void onDirectoryChanged(const QString& path);
  void onFileChanged(const QString& path);

private:
  struct RepoInventory {
    std::vector<ModRow> mods;
    std::unordered_multimap<std::string, int> nameIndex;
    std::unordered_multimap<std::string, int> steamIdIndex;
  };

  void rescanAll();
  void rescanSource(const QString& sourceKey,
                    const QString& directory,
                    RepoInventory& inventory,
                    QSet<QString>& watchedFiles,
                    QStringList& updatedMods);
  RepoInventory buildInventory() const;
  QString normalizeKey(const QString& text) const;
  QString extractWorkshopId(const std::string& url) const;
  const ModRow* findAddonMatch(const QString& normalizedName,
                               std::uint64_t fileSize,
                               RepoInventory& inventory,
                               int& matchedIndex) const;
  const ModRow* findWorkshopMatch(const QString& normalizedName,
                                  const QString& numericId,
                                  RepoInventory& inventory,
                                  int& matchedIndex) const;
  QString resolveStatus(const ModRow* mod,
                        std::uint64_t fileSizeBytes,
                        const QString& sourceKey) const;
  std::optional<QString> synchronizeWorkshopIfNeeded(const QFileInfo& fileInfo,
                                                     ModRow& modRecord,
                                                     const QString& numericId);
  QString locateWorkshopCover(const QFileInfo& fileInfo) const;
  bool copyReplacing(const QString& src, const QString& dst) const;
  std::vector<TagDescriptor> tagsForMod(int modId) const;
  void updateDirectoryWatches(const QStringList& directories);
  void updateFileWatches(const QSet<QString>& newFiles);

  QString addonsDir_;
  QString workshopDir_;
  RepositoryService* repoService_{nullptr};
  ImportService* importService_{nullptr};
  Settings settings_;
  QFileSystemWatcher watcher_;
  QStringList watchedDirectories_;
  QSet<QString> watchedFiles_;
  bool initialScanCompleted_{false};
};
