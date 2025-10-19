#include "core/repo/FixedBundleDao.h"

int FixedBundleDao::insertBundle(const std::string& name, const std::optional<std::string>& note) {
  // 插入固定搭配主记录
  Stmt stmt(*db_, "INSERT INTO fixed_bundles(name, note) VALUES(?, ?);");
  stmt.bind(1, name);
  if (note.has_value()) {
    stmt.bind(2, *note);
  } else {
    stmt.bindNull(2);
  }
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void FixedBundleDao::updateBundle(int id, const std::string& name, const std::optional<std::string>& note) {
  // 更新固定搭配的名称与备注
  Stmt stmt(*db_, "UPDATE fixed_bundles SET name = ?, note = ? WHERE id = ?;");
  stmt.bind(1, name);
  if (note.has_value()) {
    stmt.bind(2, *note);
  } else {
    stmt.bindNull(2);
  }
  stmt.bind(3, id);
  stmt.step();
}

void FixedBundleDao::deleteBundle(int id) {
  // 删除固定搭配，由数据库负责清除关联条目
  Stmt stmt(*db_, "DELETE FROM fixed_bundles WHERE id = ?;");
  stmt.bind(1, id);
  stmt.step();
}

std::vector<FixedBundleRow> FixedBundleDao::listBundles() const {
  // 返回所有固定搭配，按名称排序
  Stmt stmt(*db_, "SELECT id, name, note FROM fixed_bundles ORDER BY name;");
  std::vector<FixedBundleRow> rows;
  while (stmt.step()) {
    std::optional<std::string> note;
    if (!stmt.isNull(2)) {
      note = stmt.getText(2);
    }
    rows.push_back({stmt.getInt(0), stmt.getText(1), note});
  }
  return rows;
}

void FixedBundleDao::clearItems(int bundleId) {
  // 清空某个固定搭配的 MOD 列表
  Stmt stmt(*db_, "DELETE FROM fixed_bundle_items WHERE bundle_id = ?;");
  stmt.bind(1, bundleId);
  stmt.step();
}

void FixedBundleDao::addItem(int bundleId, int modId) {
  // 添加 MOD 映射，OR IGNORE 防止重复
  Stmt stmt(*db_, "INSERT OR IGNORE INTO fixed_bundle_items(bundle_id, mod_id) VALUES(?, ?);");
  stmt.bind(1, bundleId);
  stmt.bind(2, modId);
  stmt.step();
}

void FixedBundleDao::removeItem(int bundleId, int modId) {
  // 从固定搭配中移除指定 MOD
  Stmt stmt(*db_, "DELETE FROM fixed_bundle_items WHERE bundle_id = ? AND mod_id = ?;");
  stmt.bind(1, bundleId);
  stmt.bind(2, modId);
  stmt.step();
}

std::vector<FixedBundleItemRow> FixedBundleDao::listItems(int bundleId) const {
  // 查询固定搭配内包含的 MOD
  Stmt stmt(*db_, "SELECT bundle_id, mod_id FROM fixed_bundle_items WHERE bundle_id = ? ORDER BY mod_id;");
  stmt.bind(1, bundleId);
  std::vector<FixedBundleItemRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getInt(1)});
  }
  return rows;
}
