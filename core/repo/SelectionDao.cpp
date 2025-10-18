#include "core/repo/SelectionDao.h"

int SelectionDao::insert(const std::string& name, double budgetMb) {
  // 插入组合方案，预算采用默认单位 MB
  Stmt stmt(*db_, "INSERT INTO selections(name, budget_mb) VALUES(?, ?);");
  stmt.bind(1, name);
  stmt.bind(2, budgetMb);
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void SelectionDao::updateName(int id, const std::string& name) {
  // 更新组合名称，常见于重命名操作
  Stmt stmt(*db_, "UPDATE selections SET name = ? WHERE id = ?;");
  stmt.bind(1, name);
  stmt.bind(2, id);
  stmt.step();
}

void SelectionDao::updateBudget(int id, double budgetMb) {
  // 调整组合预算，配合随机策略预算控制
  Stmt stmt(*db_, "UPDATE selections SET budget_mb = ? WHERE id = ?;");
  stmt.bind(1, budgetMb);
  stmt.bind(2, id);
  stmt.step();
}

void SelectionDao::deleteSelection(int id) {
  // 删除组合，级联触发 selection_items ON DELETE CASCADE
  Stmt stmt(*db_, "DELETE FROM selections WHERE id = ?;");
  stmt.bind(1, id);
  stmt.step();
}

std::vector<SelectionRow> SelectionDao::listAll() const {
  // 组合列表按创建时间倒序，便于展示最近创建
  Stmt stmt(*db_, "SELECT id, name, budget_mb, created_at FROM selections ORDER BY created_at DESC;");
  std::vector<SelectionRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getText(1), stmt.getDouble(2), stmt.getText(3)});
  }
  return rows;
}

std::optional<SelectionRow> SelectionDao::findById(int id) const {
  // 按主键定位组合，返回可选值
  Stmt stmt(*db_, "SELECT id, name, budget_mb, created_at FROM selections WHERE id = ?;");
  stmt.bind(1, id);
  if (!stmt.step()) {
    return std::nullopt;
  }
  SelectionRow row{stmt.getInt(0), stmt.getText(1), stmt.getDouble(2), stmt.getText(3)};
  return row;
}

void SelectionDao::clearItems(int selectionId) {
  // 清除组合内所有条目，用于批量重写
  Stmt stmt(*db_, "DELETE FROM selection_items WHERE selection_id = ?;");
  stmt.bind(1, selectionId);
  stmt.step();
}

void SelectionDao::addItem(const SelectionItemRow& item) {
  // 写入组合条目，使用 OR REPLACE 支持锁定状态更新
  Stmt stmt(*db_, "INSERT OR REPLACE INTO selection_items(selection_id, mod_id, is_locked) VALUES(?, ?, ?);");
  stmt.bind(1, item.selection_id);
  stmt.bind(2, item.mod_id);
  stmt.bind(3, item.is_locked ? 1 : 0);
  stmt.step();
}

void SelectionDao::removeItem(int selectionId, int modId) {
  // 移除指定 MOD 的组合条目
  Stmt stmt(*db_, "DELETE FROM selection_items WHERE selection_id = ? AND mod_id = ?;");
  stmt.bind(1, selectionId);
  stmt.bind(2, modId);
  stmt.step();
}

std::vector<SelectionItemRow> SelectionDao::listItems(int selectionId) const {
  // 列出组合下的所有条目，默认按 MOD ID 排序
  Stmt stmt(*db_, R"SQL(
    SELECT selection_id, mod_id, is_locked
    FROM selection_items
    WHERE selection_id = ?
    ORDER BY mod_id;
  )SQL");
  stmt.bind(1, selectionId);
  std::vector<SelectionItemRow> items;
  while (stmt.step()) {
    items.push_back({stmt.getInt(0), stmt.getInt(1), stmt.getInt(2) != 0});
  }
  return items;
}
