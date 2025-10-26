#include "core/repo/TagDao.h"
#include <utility>

namespace {

int nextGroupPriority(Db& db) {
  Stmt stmt(db, "SELECT COALESCE(MAX(priority), 0) FROM tag_groups;");
  stmt.step();
  return stmt.getInt(0) + 10;
}

int nextTagPriority(Db& db, int groupId) {
  Stmt stmt(db, "SELECT COALESCE(MAX(priority), 0) FROM tags WHERE group_id = ?;");
  stmt.bind(1, groupId);
  stmt.step();
  return stmt.getInt(0) + 10;
}

} // namespace

int TagDao::insertGroup(const std::string& name, int priority) {
  Stmt stmt(*db_, "INSERT INTO tag_groups(name, priority) VALUES(?, ?);");
  stmt.bind(1, name);
  stmt.bind(2, priority);
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
  Stmt query(*db_, "SELECT id FROM tag_groups WHERE name = ?;");
  query.bind(1, name);
  if (!query.step()) {
    const int priority = nextGroupPriority(*db_);
    Stmt insert(*db_, "INSERT INTO tag_groups(name, priority) VALUES(?, ?);");
    insert.bind(1, name);
    insert.bind(2, priority);
    insert.step();
    return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
  }
  return query.getInt(0);
}

std::vector<TagGroupRow> TagDao::listGroups() const {
  Stmt stmt(*db_, "SELECT id, name, priority FROM tag_groups ORDER BY priority, id;");
  std::vector<TagGroupRow> rows;
  while (stmt.step()) {
    TagGroupRow row;
    row.id = stmt.getInt(0);
    row.name = stmt.getText(1);
    row.priority = stmt.getInt(2);
    rows.push_back(std::move(row));
  }
  return rows;
}

int TagDao::insertTag(int groupId, const std::string& name) {
  const int priority = nextTagPriority(*db_, groupId);
  Stmt stmt(*db_, "INSERT INTO tags(group_id, name, priority) VALUES(?, ?, ?);");
  stmt.bind(1, groupId);
  stmt.bind(2, name);
  stmt.bind(3, priority);
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
  Stmt query(*db_, "SELECT id FROM tags WHERE group_id = ? AND name = ?;");
  query.bind(1, groupId);
  query.bind(2, name);
  if (!query.step()) {
    const int priority = nextTagPriority(*db_, groupId);
    Stmt insert(*db_, "INSERT INTO tags(group_id, name, priority) VALUES(?, ?, ?);");
    insert.bind(1, groupId);
    insert.bind(2, name);
    insert.bind(3, priority);
    insert.step();
    return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
  }
  return query.getInt(0);
}

std::vector<TagRow> TagDao::listByGroup(int groupId) const {
  Stmt stmt(*db_, "SELECT id, group_id, name, priority FROM tags WHERE group_id = ? ORDER BY priority, id;");
  stmt.bind(1, groupId);
  std::vector<TagRow> rows;
  while (stmt.step()) {
    TagRow row;
    row.id = stmt.getInt(0);
    row.group_id = stmt.getInt(1);
    row.name = stmt.getText(2);
    row.priority = stmt.getInt(3);
    rows.push_back(std::move(row));
  }
  return rows;
}

std::vector<TagWithGroupRow> TagDao::listAllWithGroup() const {
  Stmt stmt(*db_, R"SQL(
    SELECT
      t.id,
      t.group_id,
      g.name,
      t.name,
      g.priority,
      t.priority
    FROM tags t
    INNER JOIN tag_groups g ON g.id = t.group_id
    ORDER BY g.priority, g.id, t.priority, t.id;
  )SQL");
  std::vector<TagWithGroupRow> rows;
  while (stmt.step()) {
    TagWithGroupRow row;
    row.id = stmt.getInt(0);
    row.group_id = stmt.getInt(1);
    row.group_name = stmt.getText(2);
    row.name = stmt.getText(3);
    row.group_priority = stmt.getInt(4);
    row.priority = stmt.getInt(5);
    rows.push_back(std::move(row));
  }
  return rows;
}

std::vector<TagWithGroupRow> TagDao::listByMod(int modId) const {
  Stmt stmt(*db_, R"SQL(
    SELECT
      t.id,
      t.group_id,
      g.name,
      t.name,
      g.priority,
      t.priority
    FROM mod_tags mt
    INNER JOIN tags t ON t.id = mt.tag_id
    INNER JOIN tag_groups g ON g.id = t.group_id
    WHERE mt.mod_id = ?
    ORDER BY g.priority, g.id, t.priority, t.id;
  )SQL");
  stmt.bind(1, modId);
  std::vector<TagWithGroupRow> rows;
  while (stmt.step()) {
    TagWithGroupRow row;
    row.id = stmt.getInt(0);
    row.group_id = stmt.getInt(1);
    row.group_name = stmt.getText(2);
    row.name = stmt.getText(3);
    row.group_priority = stmt.getInt(4);
    row.priority = stmt.getInt(5);
    rows.push_back(std::move(row));
  }
  return rows;
}

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
