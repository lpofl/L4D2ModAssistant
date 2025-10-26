#include "core/repo/CategoryDao.h"
#include <utility>

namespace {

int nextPriority(Db& db, std::optional<int> parentId) {
  if (parentId.has_value()) {
    Stmt stmt(db, "SELECT COALESCE(MAX(priority), 0) FROM categories WHERE parent_id = ?;");
    stmt.bind(1, *parentId);
    stmt.step();
    return stmt.getInt(0) + 10;
  }
  Stmt stmt(db, "SELECT COALESCE(MAX(priority), 0) FROM categories WHERE parent_id IS NULL;");
  stmt.step();
  return stmt.getInt(0) + 10;
}

} // namespace

int CategoryDao::insert(const std::string& name, std::optional<int> parentId) {
  const int priority = nextPriority(*db_, parentId);
  Stmt stmt(*db_, "INSERT INTO categories(parent_id, name, priority) VALUES(?, ?, ?);");
  if (parentId.has_value()) {
    stmt.bind(1, *parentId);
  } else {
    stmt.bindNull(1);
  }
  stmt.bind(2, name);
  stmt.bind(3, priority);
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void CategoryDao::update(int id, const std::string& name, std::optional<int> parentId, std::optional<int> priority) {
  Stmt stmt(*db_, "UPDATE categories SET parent_id = ?, name = ?, priority = COALESCE(?, priority) WHERE id = ?;");
  if (parentId.has_value()) {
    stmt.bind(1, *parentId);
  } else {
    stmt.bindNull(1);
  }
  stmt.bind(2, name);
  if (priority.has_value()) {
    stmt.bind(3, *priority);
  } else {
    stmt.bindNull(3);
  }
  stmt.bind(4, id);
  stmt.step();
}

std::vector<CategoryRow> CategoryDao::listAll() const {
  Stmt stmt(*db_, "SELECT id, parent_id, name, priority FROM categories ORDER BY COALESCE(parent_id, 0), priority, id;");
  std::vector<CategoryRow> rows;
  while (stmt.step()) {
    CategoryRow row;
    row.id = stmt.getInt(0);
    row.parent_id = stmt.isNull(1) ? std::optional<int>{} : std::optional<int>{stmt.getInt(1)};
    row.name = stmt.getText(2);
    row.priority = stmt.getInt(3);
    rows.push_back(std::move(row));
  }
  return rows;
}

std::optional<CategoryRow> CategoryDao::findById(int id) const {
  Stmt stmt(*db_, "SELECT id, parent_id, name, priority FROM categories WHERE id = ?;");
  stmt.bind(1, id);
  if (!stmt.step()) {
    return std::nullopt;
  }
  CategoryRow row;
  row.id = stmt.getInt(0);
  row.parent_id = stmt.isNull(1) ? std::optional<int>{} : std::optional<int>{stmt.getInt(1)};
  row.name = stmt.getText(2);
  row.priority = stmt.getInt(3);
  return row;
}

void CategoryDao::remove(int id) {
  Db::Tx tx(*db_);

  // Gather target category and all descendants.
  std::vector<int> pending{ id };
  std::vector<int> orderedIds; // parent before children
  while (!pending.empty()) {
    const int current = pending.back();
    pending.pop_back();
    orderedIds.push_back(current);

    Stmt children(*db_, "SELECT id FROM categories WHERE parent_id = ?;");
    children.bind(1, current);
    while (children.step()) {
      pending.push_back(children.getInt(0));
    }
  }

  // Clear category references on mods for every category in the subtree.
  for (int catId : orderedIds) {
    Stmt clearModRef(*db_, "UPDATE mods SET category_id = NULL WHERE category_id = ?;");
    clearModRef.bind(1, catId);
    clearModRef.step();
  }

  // Delete categories starting from leaves.
  for (auto it = orderedIds.rbegin(); it != orderedIds.rend(); ++it) {
    Stmt del(*db_, "DELETE FROM categories WHERE id = ?;");
    del.bind(1, *it);
    del.step();
  }

  tx.commit();
}

void CategoryDao::swapPriorities(int firstId, int secondId) {
  if (firstId == secondId) {
    return;
  }

  Db::Tx tx(*db_);

  auto fetch = [&](int id) {
    Stmt query(*db_, "SELECT parent_id, priority FROM categories WHERE id = ?;");
    query.bind(1, id);
    if (!query.step()) {
      throw DbError("category not found for swap");
    }
    std::optional<int> parent;
    if (!query.isNull(0)) {
      parent = query.getInt(0);
    }
    const int priority = query.getInt(1);
    return std::make_pair(parent, priority);
  };

  const auto [parentA, priorityA] = fetch(firstId);
  const auto [parentB, priorityB] = fetch(secondId);
  if (parentA.has_value() != parentB.has_value() ||
      (parentA.has_value() && parentA.value() != parentB.value())) {
    throw DbError("cannot swap categories from different levels");
  }

  // Use a temporary priority to avoid UNIQUE constraint conflicts.
  Stmt temp(*db_, "UPDATE categories SET priority = ? WHERE id = ?;");
  temp.bind(1, -priorityA - 1);
  temp.bind(2, firstId);
  temp.step();

  Stmt updateSecond(*db_, "UPDATE categories SET priority = ? WHERE id = ?;");
  updateSecond.bind(1, priorityA);
  updateSecond.bind(2, secondId);
  updateSecond.step();

  Stmt restoreFirst(*db_, "UPDATE categories SET priority = ? WHERE id = ?;");
  restoreFirst.bind(1, priorityB);
  restoreFirst.bind(2, firstId);
  restoreFirst.step();

  tx.commit();
}
