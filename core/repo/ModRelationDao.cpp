#include "core/repo/ModRelationDao.h"

/**
 * @file ModRelationDao.cpp
 * @brief 实现了 ModRelationDao 类中定义的方法。
 */

int ModRelationDao::insert(const ModRelationRow& row) {
  // 准备 SQL 语句，用于插入一条新的 MOD 关系记录
  Stmt stmt(*db_, R"SQL(
    INSERT INTO mod_relations(a_mod_id, b_mod_id, type, slot_key, note)
    VALUES(?, ?, ?, ?, ?);
  )SQL");
  
  // 绑定参数
  stmt.bind(1, row.a_mod_id);
  stmt.bind(2, row.b_mod_id);
  stmt.bind(3, row.type);
  
  // 绑定可选的 slot_key
  if (row.slot_key.has_value()) {
    stmt.bind(4, *row.slot_key);
  } else {
    stmt.bindNull(4);
  }
  
  // 绑定可选的 note
  if (row.note.has_value()) {
    stmt.bind(5, *row.note);
  } else {
    stmt.bindNull(5);
  }
  
  stmt.step();
  // 返回新插入行的 ID
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void ModRelationDao::removeById(int id) {
  // 根据主键 ID 删除一条关系记录
  Stmt stmt(*db_, "DELETE FROM mod_relations WHERE id = ?;");
  stmt.bind(1, id);
  stmt.step();
}

void ModRelationDao::removeBetween(int aModId, int bModId, const std::string& type) {
  // 删除两个 MOD 之间单向的、指定类型的关系 (A -> B)
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
  // 查询指定 MOD 作为关系A方或B方的所有关系记录
  Stmt stmt(*db_, R"SQL(
    SELECT id, a_mod_id, b_mod_id, type, slot_key, note
    FROM mod_relations
    WHERE a_mod_id = ? OR b_mod_id = ?
    ORDER BY type, a_mod_id, b_mod_id;
  )SQL");
  
  // 在 WHERE 子句的两个位置都绑定同一个 modId
  stmt.bind(1, modId);
  stmt.bind(2, modId);
  
  std::vector<ModRelationRow> rows;
  while (stmt.step()) {
    ModRelationRow row;
    row.id = stmt.getInt(0);
    row.a_mod_id = stmt.getInt(1);
    row.b_mod_id = stmt.getInt(2);
    row.type = stmt.getText(3);
    // 处理可选字段
    row.slot_key = stmt.isNull(4) ? std::optional<std::string>{} : std::optional<std::string>{stmt.getText(4)};
    row.note = stmt.isNull(5) ? std::optional<std::string>{} : std::optional<std::string>{stmt.getText(5)};
    rows.push_back(std::move(row));
  }
  return rows;
}