#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file SelectionDao.h
 * @brief DAO for selections and selection_items tables.
 * @note 组合方案与条目 DAO，负责保存随机策略结果。
 */

struct SelectionRow {
  int id;
  std::string name;
  double budget_mb;
  std::string created_at;
};

struct SelectionItemRow {
  int selection_id;
  int mod_id;
  bool is_locked;
};

class SelectionDao {
public:
  explicit SelectionDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /// 新建组合方案，返回主键
  int insert(const std::string& name, double budgetMb);
  /// 修改组合名称
  void updateName(int id, const std::string& name);
  /// 调整组合预算
  void updateBudget(int id, double budgetMb);
  /// 删除组合及其条目
  void deleteSelection(int id);

  /// 返回全部组合列表
  std::vector<SelectionRow> listAll() const;
  /// 按 ID 查询组合
  std::optional<SelectionRow> findById(int id) const;

  /// 清空组合下的条目
  void clearItems(int selectionId);
  /// 新增或替换组合条目
  void addItem(const SelectionItemRow& item);
  /// 删除指定条目
  void removeItem(int selectionId, int modId);
  /// 列出组合条目详情
  std::vector<SelectionItemRow> listItems(int selectionId) const;

private:
  std::shared_ptr<Db> db_;
};
