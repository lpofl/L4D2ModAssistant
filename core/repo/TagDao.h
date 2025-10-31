#pragma once
#include <memory>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file TagDao.h
 * @brief 封装了对标签组（tag_groups）、标签（tags）以及MOD与标签关联（mod_tags）的操作。
 */

/**
 * @brief 代表 tag_groups 数据表中的一行记录。
 */
struct TagGroupRow {
  int id = 0; ///< 标签组的唯一ID
  std::string name; ///< 标签组的名称
  int priority = 0; ///< 排序优先级
};

/**
 * @brief 代表 tags 数据表中的一行记录。
 */
struct TagRow {
  int id = 0; ///< 标签的唯一ID
  int group_id = 0; ///< 所属标签组的ID
  std::string name; ///< 标签的名称
  int priority = 0; ///< 组内排序优先级
};

/**
 * @brief 代表一个带有其所属组信息的标签，通常是联表查询的结果。
 */
struct TagWithGroupRow {
  int id = 0; ///< 标签的唯一ID
  int group_id = 0; ///< 标签组的ID
  std::string group_name; ///< 标签组的名称
  int group_priority = 0; ///< 标签组的排序优先级
  std::string name; ///< 标签的名称
  int priority = 0; ///< 标签的组内排序优先级
};

/**
 * @brief 标签数据访问对象（DAO）。
 * @details 提供了对标签、标签组以及它们与MOD之间关系进行操作的各种方法。
 */
class TagDao {
public:
  /**
   * @brief 构造一个新的 TagDao 对象。
   * @param db 数据库连接的共享指针。
   */
  explicit TagDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  // --- 标签组操作 ---

  /**
   * @brief 插入一个新的标签组。
   * @param name 标签组名称。
   * @param priority 排序优先级。
   * @return 新标签组的ID。
   */
  int insertGroup(const std::string& name, int priority);

  /**
   * @brief 更新标签组的名称。
   * @param groupId 要更新的标签组ID。
   * @param name 新的名称。
   */
  void updateGroup(int groupId, const std::string& name);

  /**
   * @brief 删除一个标签组。
   * @param groupId 要删除的标签组ID。
   * @return 如果成功删除（即该组下没有标签）则返回true，否则返回false。
   */
  bool removeGroup(int groupId);

  /**
   * @brief 确保一个标签组存在，如果不存在则创建它。
   * @param name 标签组的名称。
   * @return 标签组的ID。
   */
  int ensureGroupId(const std::string& name);

  /**
   * @brief 列出所有标签组。
   * @return 所有标签组的列表。
   */
  std::vector<TagGroupRow> listGroups() const;

  // --- 标签操作 ---

  /**
   * @brief 在指定组下插入一个新标签。
   * @param groupId 所属标签组的ID。
   * @param name 新标签的名称。
   * @return 新标签的ID。
   */
  int insertTag(int groupId, const std::string& name);

  /**
   * @brief 更新标签的名称。
   * @param tagId 要更新的标签ID。
   * @param name 新的名称。
   */
  void updateTag(int tagId, const std::string& name);

  /**
   * @brief 确保一个标签在指定组下存在，如果不存在则创建它。
   * @param groupId 所属标签组的ID。
   * @param name 标签的名称。
   * @return 标签的ID。
   */
  int ensureTagId(int groupId, const std::string& name);

  /**
   * @brief 列出指定组下的所有标签。
   * @param groupId 标签组ID。
   * @return 该组下所有标签的列表。
   */
  std::vector<TagRow> listByGroup(int groupId) const;

  /**
   * @brief 列出所有标签及其所属组的信息。
   * @return 包含组信息的所有标签的列表。
   */
  std::vector<TagWithGroupRow> listAllWithGroup() const;

  /**
   * @brief 列出指定MOD绑定的所有标签。
   * @param modId MOD ID。
   * @return 该MOD绑定的所有标签的列表（包含组信息）。
   */
  std::vector<TagWithGroupRow> listByMod(int modId) const;

  /**
   * @brief 删除一个未被任何MOD使用的标签。
   * @param tagId 要删除的标签ID。
   */
  void deleteUnused(int tagId);

  /**
   * @brief 删除一个标签。
   * @param tagId 要删除的标签ID。
   * @return 如果成功删除（即该标签未被使用）则返回true，否则返回false。
   */
  bool removeTag(int tagId);

  // --- MOD-标签关联操作 ---

  /**
   * @brief 清除一个MOD绑定的所有标签。
   * @param modId MOD ID。
   */
  void clearTagsForMod(int modId);

  /**
   * @brief 为一个MOD添加一个标签绑定。
   * @param modId MOD ID。
   * @param tagId 标签ID。
   */
  void addTagToMod(int modId, int tagId);

private:
  std::shared_ptr<Db> db_;
};