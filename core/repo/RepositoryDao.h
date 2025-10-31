#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file RepositoryDao.h
 * @brief 封装了对MOD仓库主表（mods）及相关视图的数据访问操作。
 */

/**
 * @brief 代表 mods 数据表中的一行记录，即一个仓库中的MOD。
 * @note 对于数据库中的可选字段，使用哨兵值（如空字符串或0）表示未设置。
 */
struct ModRow {
  int id{0}; ///< MOD的唯一ID（主键）
  std::string name; ///< MOD名称
  std::string author; ///< 作者
  int rating{0}; ///< 评级（1-5），0表示未设置
  int category_id{0}; ///< 所属分类ID，0表示未设置
  std::string note; ///< 备注
  std::string last_published_at; ///< 最后发布时间
  std::string last_saved_at; ///< 在本仓库中的最后保存时间
  std::string status{"最新"}; ///< 状态，例如 "最新", "待更新"
  std::string source_platform; ///< 来源平台，例如 "Steam Workshop"
  std::string source_url; ///< 来源网址
  bool is_deleted{false}; ///< 是否被逻辑删除
  std::string cover_path; ///< 封面图片路径
  std::string file_path; ///< MOD文件路径
  std::string file_hash; ///< MOD文件哈希值
  double size_mb{0.0}; ///< 文件大小（MB）
  std::string integrity; ///< 完整性状态
  std::string stability; ///< 稳定性状态
  std::string acquisition_method; ///< 获取方式
};

/**
 * @brief MOD仓库数据访问对象（DAO）。
 * @details 提供了对 mods 数据表进行操作的各种方法。
 */
class RepositoryDao {
public:
  /**
   * @brief 构造一个新的 RepositoryDao 对象。
   * @param db 数据库连接的共享指针。
   */
  explicit RepositoryDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /**
   * @brief 插入一条新的MOD记录。
   * @param row 包含MOD信息的 ModRow 对象（其中的id将被忽略，由数据库自动生成）。
   * @return 新插入MOD的自增主键ID。
   */
  int insertMod(const ModRow& row);

  /**
   * @brief 更新一条已存在的MOD记录的可变字段。
   * @param row 包含MOD更新信息的 ModRow 对象（必须包含有效的id）。
   */
  void updateMod(const ModRow& row);

  /**
   * @brief 设置或取消MOD的逻辑删除状态。
   * @param id 目标MOD的ID。
   * @param deleted true表示逻辑删除，false表示恢复。
   */
  void setDeleted(int id, bool deleted);

  /**
   * @brief 永久删除所有被标记为逻辑删除的MOD。
   */
  void deleteDeletedMods();

  /**
   * @brief 根据主键ID查找MOD。
   * @param id 要查找的MOD ID。
   * @return 如果找到，则返回包含MOD信息的 ModRow，否则返回 std::nullopt。
   */
  std::optional<ModRow> findById(int id) const;

  /**
   * @brief 根据文件哈希值查找MOD。
   * @param fileHash 要查找的MOD文件哈希值。
   * @return 如果找到，则返回包含MOD信息的 ModRow，否则返回 std::nullopt。
   */
  std::optional<ModRow> findByFileHash(const std::string& fileHash) const;

  /**
   * @brief 列出所有可见（即未被逻辑删除）的MOD。
   * @return 包含所有可见MOD信息的列表。
   */
  std::vector<ModRow> listVisible() const;

  /**
   * @brief 列出所有MOD。
   * @param includeDeleted 是否包含已被逻辑删除的MOD。默认为false。
   * @return 包含MOD信息的列表。
   */
  std::vector<ModRow> listAll(bool includeDeleted = false) const;

private:
  std::shared_ptr<Db> db_; ///< 数据库连接实例
};