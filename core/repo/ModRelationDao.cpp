#include "core/repo/ModRelationDao.h"

int ModRelationDao::insert(const ModRelationRow& row) {
  // 插入关系记录，槽位和备注允许为空
  Stmt stmt(*db_, R"SQL(
    INSERT INTO mod_relations(a_mod_id, b_mod_id, type, slot_key, note)
    VALUES(?, ?, ?, ?, ?);
  )SQL");
  stmt.bind(1, row.a_mod_id);
  stmt.bind(2, row.b_mod_id);
  stmt.bind(3, row.type);
  if (row.slot_key.has_value()) {
    stmt.bind(4, *row.slot_key);
  } else {
    stmt.bindNull(4);
  }
  if (row.note.has_value()) {
    stmt.bind(5, *row.note);
  } else {
    stmt.bindNull(5);
  }
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void ModRelationDao::removeById(int id) {
  // 按主键物理删除关系
  Stmt stmt(*db_, "DELETE FROM mod_relations WHERE id = ?;");
  stmt.bind(1, id);
  stmt.step();
}

void ModRelationDao::removeBetween(int aModId, int bModId, const std::string& type) {
  // 删除指定的关系对，常用于解除依赖或冲突
  Stmt stmt(*db_, R"SQL(
    DELETE FROM mod_relations
    WHERE a_mod_id = ? AND b_mod_id = ? AND type = ?;
  )SQL");
  stmt.bind(1, aModId);
  stmt.bind(2, bModId);
  stmt.bind(3, type);
  stmt.step();
}

std::vector<ModRelationRow> ModRelationDao::listByMod(int modId) const {
  // 读取与 MOD 相关的所有关系，用于前端展示和业务校验
  Stmt stmt(*db_, R"SQL(
    SELECT id, a_mod_id, b_mod_id, type, slot_key, note
    FROM mod_relations
    WHERE a_mod_id = ? OR b_mod_id = ?
    ORDER BY type, a_mod_id, b_mod_id;
  )SQL");
  stmt.bind(1, modId);
  stmt.bind(2, modId);
  std::vector<ModRelationRow> rows;
  while (stmt.step()) {
    ModRelationRow row;
    row.id = stmt.getInt(0);
    row.a_mod_id = stmt.getInt(1);
    row.b_mod_id = stmt.getInt(2);
    row.type = stmt.getText(3);
    row.slot_key = stmt.isNull(4) ? std::optional<std::string>{} : std::optional<std::string>{stmt.getText(4)};
    row.note = stmt.isNull(5) ? std::optional<std::string>{} : std::optional<std::string>{stmt.getText(5)};
    rows.push_back(std::move(row));
  }
  return rows;
}
