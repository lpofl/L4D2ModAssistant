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
 * @brief 负责维护游戏目录缓存表（gamemods）的数据访问逻辑。
 * @details 此DAO用于管理通过扫描游戏目录（如 addons, workshop）发现的MOD文件的缓存信息。
 */

/**
 * @brief 代表 gamemods 数据表中的一行记录，即一个游戏MOD文件的缓存信息。
 */
struct GameModRow {
  int id{0}; ///< 唯一ID（主键）
  std::string name; ///< MOD名称
  std::string file_path; ///< MOD文件的绝对路径
  std::string source;  ///< 来源目录，例如 "addons" 或 "workshop"
  std::uint64_t file_size{0}; ///< 文件大小（字节）
  std::string modified_at; ///< 文件最后修改时间
  std::string status; ///< MOD状态（例如，是否启用）
  std::optional<int> repo_mod_id; ///< 关联的仓库MOD ID，可以为空
  std::string last_scanned_at; ///< 最后扫描时间
};

/**
 * @brief 游戏MOD缓存数据访问对象（DAO）。
 * @details 提供了对 gamemods 数据表进行操作的各种方法。
 */
class GameModDao {
public:
  /**
   * @brief 构造一个新的 GameModDao 对象。
   * @param db 数据库连接的共享指针。
   */
  explicit GameModDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /**
   * @brief 整体替换指定来源（source）的所有MOD缓存数据。
   * @details 此操作会先删除指定来源的所有旧记录，然后插入新记录，整个过程在单个事务中完成。
   * @param source 要替换的来源目录，如 "addons"。
   * @param rows 新的MOD记录列表。
   */
  void replaceForSource(const std::string& source, const std::vector<GameModRow>& rows);

  /**
   * @brief 根据文件路径查找MOD缓存信息。
   * @param filePath 要查找的MOD文件的绝对路径。
   * @return 如果找到，则返回包含MOD信息的 GameModRow，否则返回 std::nullopt。
   */
  std::optional<GameModRow> findByPath(const std::string& filePath) const;

  /**
   * @brief 插入或更新一条MOD缓存记录。
   * @details 如果记录已存在（基于文件路径），则更新；否则，插入新记录。
   * @param row 要插入或更新的MOD记录。
   */
  void upsert(const GameModRow& row);

  /**
   * @brief 删除指定来源下，所有不在保留列表（keepPaths）中的MOD缓存。
   * @param source 目标来源目录，如 "addons"。
   * @param keepPaths 在该来源下需要保留的MOD文件路径列表。
   */
  void removeByPaths(const std::string& source, const std::vector<std::string>& keepPaths);

  /**
   * @brief 读取数据库中缓存的所有游戏MOD记录。
   * @return 包含所有MOD缓存信息的列表。
   */
  std::vector<GameModRow> listAll() const;

private:
  std::shared_ptr<Db> db_; ///< 数据库连接实例
};