#include "core/repo/SavedSchemeDao.h"

int SavedSchemeDao::insert(const std::string& name, double budgetMb) {
  Stmt stmt(*db_, "INSERT INTO saved_schemes(name, budget_mb) VALUES(?, ?);");
  stmt.bind(1, name);
  stmt.bind(2, budgetMb);
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void SavedSchemeDao::updateName(int id, const std::string& name) {
  Stmt stmt(*db_, "UPDATE saved_schemes SET name = ? WHERE id = ?;");
  stmt.bind(1, name);
  stmt.bind(2, id);
  stmt.step();
}

void SavedSchemeDao::updateBudget(int id, double budgetMb) {
  Stmt stmt(*db_, "UPDATE saved_schemes SET budget_mb = ? WHERE id = ?;");
  stmt.bind(1, budgetMb);
  stmt.bind(2, id);
  stmt.step();
}

void SavedSchemeDao::deleteScheme(int id) {
  Stmt stmt(*db_, "DELETE FROM saved_schemes WHERE id = ?;");
  stmt.bind(1, id);
  stmt.step();
}

std::vector<SavedSchemeRow> SavedSchemeDao::listAll() const {
  Stmt stmt(*db_, "SELECT id, name, budget_mb, created_at FROM saved_schemes ORDER BY created_at DESC;");
  std::vector<SavedSchemeRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getText(1), stmt.getDouble(2), stmt.getText(3)});
  }
  return rows;
}

std::optional<SavedSchemeRow> SavedSchemeDao::findById(int id) const {
  Stmt stmt(*db_, "SELECT id, name, budget_mb, created_at FROM saved_schemes WHERE id = ?;");
  stmt.bind(1, id);
  if (!stmt.step()) {
    return std::nullopt;
  }
  SavedSchemeRow row{stmt.getInt(0), stmt.getText(1), stmt.getDouble(2), stmt.getText(3)};
  return row;
}

void SavedSchemeDao::clearItems(int schemeId) {
  Stmt stmt(*db_, "DELETE FROM saved_scheme_items WHERE scheme_id = ?;");
  stmt.bind(1, schemeId);
  stmt.step();
}

void SavedSchemeDao::addItem(const SavedSchemeItemRow& item) {
  Stmt stmt(*db_, "INSERT OR REPLACE INTO saved_scheme_items(scheme_id, mod_id, is_locked) VALUES(?, ?, ?);");
  stmt.bind(1, item.scheme_id);
  stmt.bind(2, item.mod_id);
  stmt.bind(3, item.is_locked ? 1 : 0);
  stmt.step();
}

void SavedSchemeDao::removeItem(int schemeId, int modId) {
  Stmt stmt(*db_, "DELETE FROM saved_scheme_items WHERE scheme_id = ? AND mod_id = ?;");
  stmt.bind(1, schemeId);
  stmt.bind(2, modId);
  stmt.step();
}

std::vector<SavedSchemeItemRow> SavedSchemeDao::listItems(int schemeId) const {
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

