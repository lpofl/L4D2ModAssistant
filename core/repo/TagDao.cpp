#include "core/repo/TagDao.h"

// --------------------------- TAG 组操作 ---------------------------
int TagDao::insertGroup(const std::string& name, int sortOrder) {
  // 新建 TAG 组，sort_order 控制展示顺序
  Stmt stmt(*db_, "INSERT INTO tag_groups(name, sort_order) VALUES(?, ?);");
  stmt.bind(1, name);
  stmt.bind(2, sortOrder);
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

int TagDao::ensureGroupId(const std::string& name) {
  // 若组不存在则写入；若存在则忽略，保证幂等
  Stmt upsert(*db_, "INSERT OR IGNORE INTO tag_groups(name) VALUES(?);");
  upsert.bind(1, name);
  upsert.step();

  Stmt query(*db_, "SELECT id FROM tag_groups WHERE name = ?;");
  query.bind(1, name);
  if (!query.step()) {
    throw DbError("ensureGroupId failed for tag group: " + name);
  }
  return query.getInt(0);
}

std::vector<TagGroupRow> TagDao::listGroups() const {
  // 按排序权重返回所有 TAG 组，界面可直接使用
  Stmt stmt(*db_, "SELECT id, name, sort_order FROM tag_groups ORDER BY sort_order, id;");
  std::vector<TagGroupRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getText(1), stmt.getInt(2)});
  }
  return rows;
}

// --------------------------- TAG 条目操作 ---------------------------
int TagDao::insertTag(int groupId, const std::string& name) {
  // 在指定组下新增 TAG，依赖唯一约束防止重复
  Stmt stmt(*db_, "INSERT INTO tags(group_id, name) VALUES(?, ?);");
  stmt.bind(1, groupId);
  stmt.bind(2, name);
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

int TagDao::ensureTagId(int groupId, const std::string& name) {
  // 先尝试插入，若已存在则忽略，再读取主键 ID
  Stmt upsert(*db_, "INSERT OR IGNORE INTO tags(group_id, name) VALUES(?, ?);");
  upsert.bind(1, groupId);
  upsert.bind(2, name);
  upsert.step();

  Stmt query(*db_, "SELECT id FROM tags WHERE group_id = ? AND name = ?;");
  query.bind(1, groupId);
  query.bind(2, name);
  if (!query.step()) {
    throw DbError("ensureTagId failed for tag: " + name);
  }
  return query.getInt(0);
}

std::vector<TagRow> TagDao::listByGroup(int groupId) const {
  // 获取指定组内的 TAG 列表，按名称排序便于查找
  Stmt stmt(*db_, "SELECT id, group_id, name FROM tags WHERE group_id = ? ORDER BY name;");
  stmt.bind(1, groupId);
  std::vector<TagRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getInt(1), stmt.getText(2)});
  }
  return rows;
}

std::vector<TagWithGroupRow> TagDao::listAllWithGroup() const {
  // 联表返回 TAG + 组名，供管理界面一次性展示
  Stmt stmt(*db_, R"SQL(
    SELECT t.id, t.group_id, g.name, t.name
    FROM tags t
    INNER JOIN tag_groups g ON g.id = t.group_id
    ORDER BY g.sort_order, g.id, t.name;
  )SQL");
  std::vector<TagWithGroupRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getInt(1), stmt.getText(2), stmt.getText(3)});
  }
  return rows;
}

std::vector<TagWithGroupRow> TagDao::listByMod(int modId) const {
  // 查询 MOD 已绑定的 TAG，同时附带组信息用于展示/过滤
  Stmt stmt(*db_, R"SQL(
    SELECT t.id, t.group_id, g.name, t.name
    FROM mod_tags mt
    INNER JOIN tags t ON t.id = mt.tag_id
    INNER JOIN tag_groups g ON g.id = t.group_id
    WHERE mt.mod_id = ?
    ORDER BY g.sort_order, g.id, t.name;
  )SQL");
  stmt.bind(1, modId);
  std::vector<TagWithGroupRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getInt(1), stmt.getText(2), stmt.getText(3)});
  }
  return rows;
}

// --------------------------- TAG 关系操作 ---------------------------
void TagDao::deleteUnused(int tagId) {
  // 仅当没有 MOD 引用时才删除 TAG，避免误删在用数据
  Stmt check(*db_, "SELECT COUNT(*) FROM mod_tags WHERE tag_id = ?;");
  check.bind(1, tagId);
  if (check.step() && check.getInt(0) == 0) {
    Stmt del(*db_, "DELETE FROM tags WHERE id = ?;");
    del.bind(1, tagId);
    del.step();
  }
}

void TagDao::clearTagsForMod(int modId) {
  // 重建 MOD TAG 映射前，先清空历史数据
  Stmt stmt(*db_, "DELETE FROM mod_tags WHERE mod_id = ?;");
  stmt.bind(1, modId);
  stmt.step();
}

void TagDao::addTagToMod(int modId, int tagId) {
  // 使用 OR IGNORE，避免重复绑定导致异常
  Stmt stmt(*db_, "INSERT OR IGNORE INTO mod_tags(mod_id, tag_id) VALUES(?, ?);");
  stmt.bind(1, modId);
  stmt.bind(2, tagId);
  stmt.step();
}
