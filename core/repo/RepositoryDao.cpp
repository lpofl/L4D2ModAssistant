#include "core/repo/RepositoryDao.h"

int RepositoryDao::insertMod(const ModRow& row) {
  // 写入 MOD 主表，根据字段值决定是否绑定 NULL
  Stmt stmt(*db_, R"SQL(
    INSERT INTO mods(name, rating, category_id, size_mb, is_deleted, file_path, file_hash)
    VALUES(?, ?, ?, ?, ?, ?, ?);
  )SQL");
  stmt.bind(1, row.name);
  if (row.rating > 0) {
    stmt.bind(2, row.rating);
  } else {
    stmt.bindNull(2);
  }
  if (row.category_id > 0) {
    stmt.bind(3, row.category_id);
  } else {
    stmt.bindNull(3);
  }
  stmt.bind(4, row.size_mb);
  stmt.bind(5, row.is_deleted ? 1 : 0);
  if (!row.file_path.empty()) {
    stmt.bind(6, row.file_path);
  } else {
    stmt.bindNull(6);
  }
  if (!row.file_hash.empty()) {
    stmt.bind(7, row.file_hash);
  } else {
    stmt.bindNull(7);
  }
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

std::optional<ModRow> RepositoryDao::findById(int id) const {
  // 按 ID 查询单条记录，缺省字段使用 COALESCE 转换为默认值
  Stmt stmt(*db_, R"SQL(
    SELECT id, name, COALESCE(rating, 0), COALESCE(category_id, 0), size_mb, is_deleted,
           COALESCE(file_path, ''), COALESCE(file_hash, '')
    FROM mods
    WHERE id = ?;
  )SQL");
  stmt.bind(1, id);
  if (!stmt.step()) {
    return std::nullopt;
  }
  return ModRow{
    stmt.getInt(0),
    stmt.getText(1),
    stmt.getInt(2),
    stmt.getInt(3),
    stmt.getDouble(4),
    stmt.getInt(5) != 0,
    stmt.getText(6),
    stmt.getText(7)
  };
}

std::vector<ModRow> RepositoryDao::listVisible() const {
  // 从视图 v_mods_visible 读取逻辑未删除的记录
  Stmt stmt(*db_, R"SQL(
    SELECT id, name, COALESCE(rating, 0), COALESCE(category_id, 0), size_mb,
           is_deleted, COALESCE(file_path, ''), COALESCE(file_hash, '')
    FROM v_mods_visible
    ORDER BY name;
  )SQL");
  std::vector<ModRow> out;
  while (stmt.step()) {
    out.push_back({
      stmt.getInt(0),
      stmt.getText(1),
      stmt.getInt(2),
      stmt.getInt(3),
      stmt.getDouble(4),
      stmt.getInt(5) != 0,
      stmt.getText(6),
      stmt.getText(7)
    });
  }
  return out;
}
