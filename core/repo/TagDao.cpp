#include "core/repo/TagDao.h"

int TagDao::insert(const std::string& name) {
  // 直接写入 TAG 词典，依赖 UNIQUE 约束防止重复
  Stmt stmt(*db_, "INSERT INTO tags(name) VALUES(?);");
  stmt.bind(1, name);
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

int TagDao::ensureTagId(const std::string& name) {
  // 先尝试插入（如果存在则忽略），再查询 ID，保证线程安全
  Stmt upsert(*db_, "INSERT OR IGNORE INTO tags(name) VALUES(?);");
  upsert.bind(1, name);
  upsert.step();

  Stmt query(*db_, "SELECT id FROM tags WHERE name = ?;");
  query.bind(1, name);
  if (!query.step()) {
    throw DbError("ensureTagId failed for tag: " + name);
  }
  return query.getInt(0);
}

std::vector<TagRow> TagDao::listAll() const {
  // 按名字排序返回所有 TAG
  Stmt stmt(*db_, "SELECT id, name FROM tags ORDER BY name;");
  std::vector<TagRow> out;
  while (stmt.step()) {
    out.push_back({stmt.getInt(0), stmt.getText(1)});
  }
  return out;
}

std::vector<TagRow> TagDao::listByMod(int modId) const {
  // 通过关联表读取 MOD 的 TAG 列表
  Stmt stmt(*db_, R"SQL(
    SELECT t.id, t.name
    FROM tags t
    INNER JOIN mod_tags mt ON mt.tag_id = t.id
    WHERE mt.mod_id = ?
    ORDER BY t.name;
  )SQL");
  stmt.bind(1, modId);
  std::vector<TagRow> out;
  while (stmt.step()) {
    out.push_back({stmt.getInt(0), stmt.getText(1)});
  }
  return out;
}

void TagDao::deleteUnused(int tagId) {
  // 若 TAG 未被任何 MOD 引用，则安全删除
  Stmt check(*db_, "SELECT COUNT(*) FROM mod_tags WHERE tag_id = ?;");
  check.bind(1, tagId);
  if (check.step() && check.getInt(0) == 0) {
    Stmt del(*db_, "DELETE FROM tags WHERE id = ?;");
    del.bind(1, tagId);
    del.step();
  }
}

void TagDao::clearTagsForMod(int modId) {
  // 清除 MOD 与 TAG 的全部映射
  Stmt stmt(*db_, "DELETE FROM mod_tags WHERE mod_id = ?;");
  stmt.bind(1, modId);
  stmt.step();
}

void TagDao::addTagToMod(int modId, int tagId) {
  // 添加 MOD 与 TAG 的映射关系，利用 OR IGNORE 避免重复
  Stmt stmt(*db_, "INSERT OR IGNORE INTO mod_tags(mod_id, tag_id) VALUES(?, ?);");
  stmt.bind(1, modId);
  stmt.bind(2, tagId);
  stmt.step();
}
