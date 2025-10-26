#include "core/repo/TagDao.h"

// --------------------------- TAG 组操作 ---------------------------

int TagDao::insertGroup(const std::string& name, int sortOrder) {
  Stmt stmt(*db_, "INSERT INTO tag_groups(name, sort_order) VALUES(?, ?);");
  stmt.bind(1, name);
  stmt.bind(2, sortOrder);
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void TagDao::updateGroup(int groupId, const std::string& name) {
  Stmt stmt(*db_, "UPDATE tag_groups SET name = ? WHERE id = ?;");
  stmt.bind(1, name);
  stmt.bind(2, groupId);
  stmt.step();
}

bool TagDao::removeGroup(int groupId) {
  Db::Tx tx(*db_);

  {
    Stmt count(*db_, "SELECT COUNT(*) FROM tags WHERE group_id = ?;");
    count.bind(1, groupId);
    if (count.step() && count.getInt(0) > 0) {
      return false;
    }
  }

  {
    Stmt stmt(*db_, "DELETE FROM tag_groups WHERE id = ?;");
    stmt.bind(1, groupId);
    stmt.step();
  }

  tx.commit();
  return true;
}

int TagDao::ensureGroupId(const std::string& name) {
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
  Stmt stmt(*db_, "SELECT id, name, sort_order FROM tag_groups ORDER BY sort_order, id;");
  std::vector<TagGroupRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getText(1), stmt.getInt(2)});
  }
  return rows;
}

// --------------------------- TAG 条目操作 ---------------------------

int TagDao::insertTag(int groupId, const std::string& name) {
  Stmt stmt(*db_, "INSERT INTO tags(group_id, name) VALUES(?, ?);");
  stmt.bind(1, groupId);
  stmt.bind(2, name);
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void TagDao::updateTag(int tagId, const std::string& name) {
  Stmt stmt(*db_, "UPDATE tags SET name = ? WHERE id = ?;");
  stmt.bind(1, name);
  stmt.bind(2, tagId);
  stmt.step();
}

int TagDao::ensureTagId(int groupId, const std::string& name) {
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
  Stmt stmt(*db_, "SELECT id, group_id, name FROM tags WHERE group_id = ? ORDER BY name;");
  stmt.bind(1, groupId);
  std::vector<TagRow> rows;
  while (stmt.step()) {
    rows.push_back({stmt.getInt(0), stmt.getInt(1), stmt.getText(2)});
  }
  return rows;
}

std::vector<TagWithGroupRow> TagDao::listAllWithGroup() const {
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
  Stmt check(*db_, "SELECT COUNT(*) FROM mod_tags WHERE tag_id = ?;");
  check.bind(1, tagId);
  if (check.step() && check.getInt(0) == 0) {
    Stmt del(*db_, "DELETE FROM tags WHERE id = ?;");
    del.bind(1, tagId);
    del.step();
  }
}

bool TagDao::removeTag(int tagId) {
  Db::Tx tx(*db_);

  {
    Stmt check(*db_, "SELECT COUNT(*) FROM mod_tags WHERE tag_id = ?;");
    check.bind(1, tagId);
    if (check.step() && check.getInt(0) > 0) {
      return false;
    }
  }

  {
    Stmt stmt(*db_, "DELETE FROM tags WHERE id = ?;");
    stmt.bind(1, tagId);
    stmt.step();
  }

  tx.commit();
  return true;
}

void TagDao::clearTagsForMod(int modId) {
  Stmt stmt(*db_, "DELETE FROM mod_tags WHERE mod_id = ?;");
  stmt.bind(1, modId);
  stmt.step();
}

void TagDao::addTagToMod(int modId, int tagId) {
  Stmt stmt(*db_, "INSERT OR IGNORE INTO mod_tags(mod_id, tag_id) VALUES(?, ?);");
  stmt.bind(1, modId);
  stmt.bind(2, tagId);
  stmt.step();
}
