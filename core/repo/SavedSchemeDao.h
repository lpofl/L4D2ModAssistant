#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file SavedSchemeDao.h
 * @brief 封装了对“已存方案”(Saved Scheme)相关数据表的操作。
 * @details 此DAO负责管理 saved_schemes 和 saved_scheme_items 两个表。
 */

/**
 * @brief 代表 saved_schemes 数据表中的一行记录，即一个已存方案的元信息。
 */
struct SavedSchemeRow {
  int id; ///< 方案的唯一ID
  std::string name; ///< 方案名称
  double budget_mb; ///< 方案的预算大小（MB）
  std::string created_at; ///< 创建时间
};

/**
 * @brief 代表 saved_scheme_items 数据表中的一行记录，即方案中包含的一个MOD。
 */
struct SavedSchemeItemRow {
  int scheme_id; ///< 所属方案的ID
  int mod_id;    ///< 包含的MOD ID
  bool is_locked; ///< 该MOD在方案中是否被锁定（例如，在随机化时不能被替换）
};

/**
 * @brief 已存方案数据访问对象（DAO）。
 * @details 提供了对 saved_schemes 和 saved_scheme_items 数据表进行操作的各种方法。
 */
class SavedSchemeDao {
public:
  /**
   * @brief 构造一个新的 SavedSchemeDao 对象。
   * @param db 数据库连接的共享指针。
   */
  explicit SavedSchemeDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /**
   * @brief 创建一个新的已存方案。
   * @param name 方案名称。
   * @param budgetMb 方案的预算（MB）。
   * @return 新创建方案的ID。
   */
  int insert(const std::string& name, double budgetMb);
  
  /**
   * @brief 更新方案的名称。
   * @param id 要更新的方案ID。
   * @param name 新的名称。
   */
  void updateName(int id, const std::string& name);
  
  /**
   * @brief 更新方案的预算。
   * @param id 要更新的方案ID。
   * @param budgetMb 新的预算（MB）。
   */
  void updateBudget(int id, double budgetMb);
  
  /**
   * @brief 删除一个方案及其包含的所有条目。
   * @details 此操作会级联删除该方案下的所有MOD关联条目。
   * @param id 要删除的方案ID。
   */
  void deleteScheme(int id);

  /**
   * @brief 列出所有已保存的方案。
   * @return 包含所有方案元信息的列表。
   */
  std::vector<SavedSchemeRow> listAll() const;
  
  /**
   * @brief 根据ID查找一个方案。
   * @param id 要查找的方案ID。
   * @return 如果找到，则返回方案信息，否则返回 std::nullopt。
   */
  std::optional<SavedSchemeRow> findById(int id) const;

  /**
   * @brief 清空一个方案中的所有MOD条目。
   * @param schemeId 要清空的方案ID。
   */
  void clearItems(int schemeId);
  
  /**
   * @brief 向方案中添加一个MOD条目。
   * @details 如果条目已存在，则会忽略。
   * @param item 要添加的条目信息。
   */
  void addItem(const SavedSchemeItemRow& item);
  
  /**
   * @brief 从方案中移除一个MOD条目。
   * @param schemeId 目标方案的ID。
   * @param modId 要移除的MOD ID。
   */
  void removeItem(int schemeId, int modId);
  
  /**
   * @brief 列出一个方案中包含的所有MOD条目。
   * @param schemeId 要查询的方案ID。
   * @return 包含所有MOD条目信息的列表。
   */
  std::vector<SavedSchemeItemRow> listItems(int schemeId) const;

private:
  std::shared_ptr<Db> db_; ///< 数据库连接实例
};