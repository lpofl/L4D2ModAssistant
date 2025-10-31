#include "core/repo/FixedBundleDao.h"

/**
 * @file FixedBundleDao.cpp
 * @brief 实现了 FixedBundleDao 类中定义的方法。
 */

int FixedBundleDao::insertBundle(const std::string& name, const std::optional<std::string>& note) {
  // 准备 SQL 语句，用于插入一条新的固定搭配记录
  Stmt stmt(*db_, "INSERT INTO fixed_bundles(name, note) VALUES(?, ?);");
  stmt.bind(1, name);
  
  // 绑定备注信息，如果存在的话
  if (note.has_value()) {
    stmt.bind(2, *note);
  } else {
    stmt.bindNull(2);
  }
  
  stmt.step();
  // 返回新插入行的 ID
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void FixedBundleDao::updateBundle(int id, const std::string& name, const std::optional<std::string>& note) {
  // 准备 SQL 语句，用于更新指定 ID 的固定搭配
  Stmt stmt(*db_, "UPDATE fixed_bundles SET name = ?, note = ? WHERE id = ?;");
  stmt.bind(1, name);
  
  // 绑定备注信息，如果存在的话
  if (note.has_value()) {
    stmt.bind(2, *note);
  } else {
    stmt.bindNull(2);
  }
  
  stmt.bind(3, id);
  stmt.step();
}

void FixedBundleDao::deleteBundle(int id) {
  // 准备 SQL 语句，删除指定 ID 的固定搭配
  // 数据库的外键约束设置了 ON DELETE CASCADE，会自动删除 fixed_bundle_items 中关联的条目
  Stmt stmt(*db_, "DELETE FROM fixed_bundles WHERE id = ?;");
  stmt.bind(1, id);
  stmt.step();
}

std::vector<FixedBundleRow> FixedBundleDao::listBundles() const {
  // 查询所有的固定搭配，并按名称排序
  Stmt stmt(*db_, "SELECT id, name, note FROM fixed_bundles ORDER BY name;");
  std::vector<FixedBundleRow> rows;
  while (stmt.step()) {
    std::optional<std::string> note;
    // 检查 note 字段是否为 NULL
    if (!stmt.isNull(2)) {
      note = stmt.getText(2);
    }
    rows.push_back({stmt.getInt(0), stmt.getText(1), note});
  }
  return rows;
}

void FixedBundleDao::clearItems(int bundleId) {
  // 删除指定固定搭配 ID 的所有 MOD 关联条目
  Stmt stmt(*db_, "DELETE FROM fixed_bundle_items WHERE bundle_id = ?;");
  stmt.bind(1, bundleId);
  stmt.step();
}

void FixedBundleDao::addItem(int bundleId, int modId) {
  // "INSERT OR IGNORE" 会在插入的记录已存在（违反 UNIQUE 约束）时，直接忽略该操作，不会报错
  Stmt stmt(*db_, "INSERT OR IGNORE INTO fixed_bundle_items(bundle_id, mod_id) VALUES(?, ?);");
  stmt.bind(1, bundleId);
  stmt.bind(2, modId);
  stmt.step();
}

void FixedBundleDao::removeItem(int bundleId, int modId) {
  // 删除指定的 MOD 和固定搭配的关联
  Stmt stmt(*db_, "DELETE FROM fixed_bundle_items WHERE bundle_id = ? AND mod_id = ?;");
  stmt.bind(1, bundleId);
  stmt.bind(2, modId);
  stmt.step();
}

std::vector<FixedBundleItemRow> FixedBundleDao::listItems(int bundleId) const {
  // 查询指定固定搭配包含的所有 MOD 条目，并按 MOD ID 排序
  Stmt stmt(*db_, "SELECT bundle_id, mod_id FROM fixed_bundle_items WHERE bundle_id = ? ORDER BY mod_id;");
  stmt.bind(1, bundleId);
  std::vector<FixedBundleItemRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getInt(1)});
  }
  return rows;
}