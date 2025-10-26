#pragma once
#include <memory>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file TagDao.h
 * @brief DAO for managing tag groups, tags, and mod-tag relations.
 * @note TAG 采用“组 + 条目”的二级结构，所有写入操作需维护数据一致性。
 */

 //一级TAG目录
struct TagGroupRow {
  int id;
  std::string name;
  int sort_order;//一级目录排序
};

/// 单条 TAG 记录（仅含所属组 ID）
struct TagRow {
  int id;
  int group_id;
  std::string name;
};

/// 附带组名的 TAG 记录，便于界面展示
struct TagWithGroupRow {
  int id;
  int group_id;
  std::string group_name;
  std::string name;
};

class TagDao {
public:
  explicit TagDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /// 新建 TAG 组，可指定排序权重
  int insertGroup(const std::string& name, int sortOrder);
  /// 更新 TAG 组名称
  void updateGroup(int groupId, const std::string& name);
  /// 删除 TAG 组（组内必须无标签）
  bool removeGroup(int groupId);
  /// 保证 TAG 组存在，不存在则创建
  int ensureGroupId(const std::string& name);
  /// 按 sort_order 列表返回所有 TAG 组
  std::vector<TagGroupRow> listGroups() const;

  /// 在指定组下新增 TAG
  int insertTag(int groupId, const std::string& name);
  /// 更新 TAG 名称
  void updateTag(int tagId, const std::string& name);
  /// 确保指定组内的 TAG 存在，返回 ID
  int ensureTagId(int groupId, const std::string& name);
  /// 获取某个组下的全部 TAG
  std::vector<TagRow> listByGroup(int groupId) const;
  /// 获取全量 TAG，包含所属组信息
  std::vector<TagWithGroupRow> listAllWithGroup() const;
  /// 获取某个 MOD 绑定的 TAG（附带组信息）
  std::vector<TagWithGroupRow> listByMod(int modId) const;
  /// 删除未被引用的 TAG
  void deleteUnused(int tagId);
  /// 删除 TAG（不可被引用）
  bool removeTag(int tagId);

  /// 清空 MOD 的 TAG 映射
  void clearTagsForMod(int modId);
  /// 为 MOD 添加 TAG 映射关系
  void addTagToMod(int modId, int tagId);

private:
  std::shared_ptr<Db> db_;
};
