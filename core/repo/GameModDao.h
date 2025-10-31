#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file GameModDao.h
 * @brief 负责维护游戏目录缓存表 gamemods 的数据访问逻辑。
 */

/// 表示一条 gamemods 记录的简单数据结构。
struct GameModRow {
  int id{0};
  std::string name;
  std::string file_path;
  std::string source;  ///< addons 或 workshop
  std::uint64_t file_size{0};
  std::string modified_at;
  std::string status;
  std::optional<int> repo_mod_id;
  std::string last_scanned_at;
};

class GameModDao {
public:
  explicit GameModDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /// 按来源目录（addons/workshop）整体替换缓存数据。
  void replaceForSource(const std::string& source, const std::vector<GameModRow>& rows);

  /// 根据文件路径查找缓存信息。
  std::optional<GameModRow> findByPath(const std::string& filePath) const;

  /// 单条写入或更新缓存记录。
  void upsert(const GameModRow& row);

  /// 删除指定来源下不存在的缓存记录。
  void removeByPaths(const std::string& source, const std::vector<std::string>& keepPaths);

  /// 读取全部游戏目录缓存。
  std::vector<GameModRow> listAll() const;

private:
  std::shared_ptr<Db> db_;
};
