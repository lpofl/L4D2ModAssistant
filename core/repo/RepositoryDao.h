
#pragma once
#include <vector>
#include <string>
#include <memory>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file RepositoryDao.h
 * @brief 仓库数据访问层，负责对 mods 等表的增删查。
 */

/**
 * @brief mods 视图/表的一行记录。
 */
struct ModRow {
  int id;                 ///< 主键 ID
  std::string name;       ///< Mod 名称
  int rating;             ///< 评分（1-5，允许 0 代表缺省）
  int category_id;        ///< 分类 ID（0 代表未知/未设置）
  double size_mb;         ///< 文件大小（MB）
  bool is_deleted;        ///< 是否已删除（逻辑删除标志）
  std::string file_path;  ///< 文件路径（可能为空）
  std::string file_hash;  ///< 文件哈希（唯一约束）
};

/**
 * @brief 与仓库相关的 DAO。
 */
class RepositoryDao {
public:
  /**
   * @brief 构造函数。
   * @param db 数据库句柄共享指针。
   */
   // std::shared_ptr 智能指针，用于管理Db对象的内存
   // std::move(db) 是把参数 db 的所有权移动给成员变量 db_
  explicit RepositoryDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /**
   * @brief 插入一条 Mod 记录。
   * @param m 要插入的记录内容。
   * @return 新记录的自增 ID。
   */
  int insertMod(const ModRow& m);

  /**
   * @brief 查询所有未删除的 Mod 列表（对用户可见）。
   * @return ModRow 列表。
   */
  std::vector<ModRow> listVisible();
private:
  std::shared_ptr<Db> db_; ///< 底层数据库连接。
};
