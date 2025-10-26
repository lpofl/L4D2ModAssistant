#include "core/repo/CategoryDao.h"

int CategoryDao::insert(const std::string& name, std::optional<int> parentId) {
  // 新增分类类型，一级分类父ID为空，写入 NULL
  Stmt stmt(*db_, "INSERT INTO categories(parent_id, name) VALUES(?, ?);");
  if (parentId.has_value()) {
    stmt.bind(1, *parentId);
  } else {
    stmt.bindNull(1);
  }
  stmt.bind(2, name);
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));// 返回自增主键
}

void CategoryDao::update(int id, const std::string& name, std::optional<int> parentId) {
  // 更新分类的名称及父节点，确保空父节点写入 NULL
  Stmt stmt(*db_, "UPDATE categories SET parent_id = ?, name = ? WHERE id = ?;");
  if (parentId.has_value()) {
    stmt.bind(1, *parentId);
  } else {
    stmt.bindNull(1);
  }
  stmt.bind(2, name);
  stmt.bind(3, id);
  stmt.step();
}

std::vector<CategoryRow> CategoryDao::listAll() const {
  // 统一输出所有分类；按照父节点和名称排序便于构造树结构
  Stmt stmt(*db_, "SELECT id, parent_id, name FROM categories ORDER BY COALESCE(parent_id, 0), name;");
  std::vector<CategoryRow> rows;
  while (stmt.step()) {
    CategoryRow row;
    row.id = stmt.getInt(0);
    row.parent_id = stmt.isNull(1) ? std::optional<int>{} : std::optional<int>{stmt.getInt(1)};
    row.name = stmt.getText(2);
    rows.push_back(std::move(row));
  }
  return rows;
}

std::optional<CategoryRow> CategoryDao::findById(int id) const {
  // 按主键读取分类，返回 std::nullopt 表示不存在
  Stmt stmt(*db_, "SELECT id, parent_id, name FROM categories WHERE id = ?;");
  stmt.bind(1, id);
  if (!stmt.step()) {
    return std::nullopt;
  }
  CategoryRow row;
  row.id = stmt.getInt(0);
  row.parent_id = stmt.isNull(1) ? std::optional<int>{} : std::optional<int>{stmt.getInt(1)};
  row.name = stmt.getText(2);
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
