#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file TagDao.h
 * @brief DAO for tag dictionary and mod-tag associations.
 * @note TAG 词典及模组 TAG 关系维护。
 */

struct TagRow {
  int id;
  std::string name;
};

class TagDao {
public:
  explicit TagDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /// 插入新 TAG，返回主键
  int insert(const std::string& name);
  /// 确保 TAG 存在，若不存在先插入再返回 ID
  int ensureTagId(const std::string& name);
  /// 返回所有 TAG 列表
  std::vector<TagRow> listAll() const;
  /// 返回指定 MOD 绑定的 TAG
  std::vector<TagRow> listByMod(int modId) const;
  /// 删除未被引用的 TAG（脏数据清理）
  void deleteUnused(int tagId);

  /// 清空指定 MOD 的 TAG 映射
  void clearTagsForMod(int modId);
  /// 为 MOD 追加一个 TAG 绑定
  void addTagToMod(int modId, int tagId);

private:
  std::shared_ptr<Db> db_;
};
