#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file SavedSchemeDao.h
 * @brief DAO for saved_schemes and saved_scheme_items tables.
 */

/// 保存的组合方案实体（仅包含方案元信息）
struct SavedSchemeRow {
  int id;
  std::string name;
  double budget_mb;
  std::string created_at;
};

/// 组合方案内的具体 MOD 记录
struct SavedSchemeItemRow {
  int scheme_id;
  int mod_id;
  bool is_locked;
};

class SavedSchemeDao {
public:
  explicit SavedSchemeDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /// 创建组合方案，返回主键 ID
  int insert(const std::string& name, double budgetMb);
  /// 修改方案名称
  void updateName(int id, const std::string& name);
  /// 修改方案预算
  void updateBudget(int id, double budgetMb);
  /// 删除方案及其条目
  void deleteScheme(int id);

  /// 列出所有已保存的组合方案
  std::vector<SavedSchemeRow> listAll() const;
  /// 按主键查询某个组合方案
  std::optional<SavedSchemeRow> findById(int id) const;

  /// 清空方案下的全部条目
  void clearItems(int schemeId);
  /// 新增或更新方案条目
  void addItem(const SavedSchemeItemRow& item);
  /// 删除指定条目
  void removeItem(int schemeId, int modId);
  /// 查询方案包含的 MOD 列表
  std::vector<SavedSchemeItemRow> listItems(int schemeId) const;

private:
  std::shared_ptr<Db> db_;
};
