#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file RepositoryDao.h
 * @brief Data access helpers for the mods table and related views.
 * @note 模组主表 DAO，封装基础增查。
 */

struct ModRow {
  int id;
  std::string name;
  int rating;
  int category_id;
  double size_mb;
  bool is_deleted;
  std::string file_path;
  std::string file_hash;
};

class RepositoryDao {
public:
  explicit RepositoryDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /// 插入 MOD 记录，同时处理可空字段
  int insertMod(const ModRow& row);
  /// 按主键查询 MOD
  std::optional<ModRow> findById(int id) const;
  /// 查询可见（未删除） MOD 列表
  std::vector<ModRow> listVisible() const;

private:
  std::shared_ptr<Db> db_;
};
