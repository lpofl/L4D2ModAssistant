#include "core/repo/SavedSchemeDao.h"

/**
 * @file SavedSchemeDao.cpp
 * @brief 实现了 SavedSchemeDao 类中定义的方法。
 */

int SavedSchemeDao::insert(const std::string& name, double budgetMb) {
  // 准备 SQL 语句，用于插入一条新的方案记录
  Stmt stmt(*db_, "INSERT INTO saved_schemes(name, budget_mb) VALUES(?, ?);");
  stmt.bind(1, name);
  stmt.bind(2, budgetMb);
  stmt.step();
  // 返回新插入行的 ID
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void SavedSchemeDao::updateName(int id, const std::string& name) {
  // 更新指定方案的名称
  Stmt stmt(*db_, "UPDATE saved_schemes SET name = ? WHERE id = ?;");
  stmt.bind(1, name);
  stmt.bind(2, id);
  stmt.step();
}

void SavedSchemeDao::updateBudget(int id, double budgetMb) {
  // 更新指定方案的预算
  Stmt stmt(*db_, "UPDATE saved_schemes SET budget_mb = ? WHERE id = ?;");
  stmt.bind(1, budgetMb);
  stmt.bind(2, id);
  stmt.step();
}

void SavedSchemeDao::deleteScheme(int id) {
  // 删除指定ID的方案
  // 数据库的外键约束设置了 ON DELETE CASCADE，会自动删除 saved_scheme_items 中关联的条目
  Stmt stmt(*db_, "DELETE FROM saved_schemes WHERE id = ?;");
  stmt.bind(1, id);
  stmt.step();
}

std::vector<SavedSchemeRow> SavedSchemeDao::listAll() const {
  // 查询所有已存方案，按创建时间降序排列
  Stmt stmt(*db_, "SELECT id, name, budget_mb, created_at FROM saved_schemes ORDER BY created_at DESC;");
  std::vector<SavedSchemeRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getText(1), stmt.getDouble(2), stmt.getText(3)});
  }
  return rows;
}

std::optional<SavedSchemeRow> SavedSchemeDao::findById(int id) const {
  // 根据主键ID查询单个方案
  Stmt stmt(*db_, "SELECT id, name, budget_mb, created_at FROM saved_schemes WHERE id = ?;");
  stmt.bind(1, id);
  if (!stmt.step()) {
    return std::nullopt;
  }
  SavedSchemeRow row{stmt.getInt(0), stmt.getText(1), stmt.getDouble(2), stmt.getText(3)};
  return row;
}

void SavedSchemeDao::clearItems(int schemeId) {
  // 删除指定方案下的所有MOD条目
  Stmt stmt(*db_, "DELETE FROM saved_scheme_items WHERE scheme_id = ?;");
  stmt.bind(1, schemeId);
  stmt.step();
}

void SavedSchemeDao::addItem(const SavedSchemeItemRow& item) {
  // 使用 "INSERT OR REPLACE" 插入或更新条目。
  // 如果 scheme_id 和 mod_id 的组合已存在，则会删除旧行并插入新行，从而更新 is_locked 状态。
  Stmt stmt(*db_, "INSERT OR REPLACE INTO saved_scheme_items(scheme_id, mod_id, is_locked) VALUES(?, ?, ?);");
  stmt.bind(1, item.scheme_id);
  stmt.bind(2, item.mod_id);
  stmt.bind(3, item.is_locked ? 1 : 0);
  stmt.step();
}

void SavedSchemeDao::removeItem(int schemeId, int modId) {
  // 从方案中删除指定的MOD条目
  Stmt stmt(*db_, "DELETE FROM saved_scheme_items WHERE scheme_id = ? AND mod_id = ?;");
  stmt.bind(1, schemeId);
  stmt.bind(2, modId);
  stmt.step();
}

std::vector<SavedSchemeItemRow> SavedSchemeDao::listItems(int schemeId) const {
  // 查询指定方案包含的所有MOD条目，并按MOD ID排序
  Stmt stmt(*db_, R"SQL(
    SELECT scheme_id, mod_id, is_locked
    FROM saved_scheme_items
    WHERE scheme_id = ?
    ORDER BY mod_id;
  )SQL");
  stmt.bind(1, schemeId);
  std::vector<SavedSchemeItemRow> items;
  while (stmt.step()) {
    items.push_back({stmt.getInt(0), stmt.getInt(1), stmt.getInt(2) != 0});
  }
  return items;
}