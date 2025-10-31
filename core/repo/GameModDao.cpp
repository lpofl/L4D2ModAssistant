#include "core/repo/GameModDao.h"

namespace {

/// 将可选的整型字段写入 SQLite 语句。
inline void bindOptionalInt(Stmt& stmt, int index, const std::optional<int>& value) {
  if (value.has_value()) {
    stmt.bind(index, value.value());
  } else {
    stmt.bindNull(index);
  }
}

}  // namespace

void GameModDao::replaceForSource(const std::string& source, const std::vector<GameModRow>& rows) {
  Db::Tx tx(*db_);
  Stmt del(*db_, "DELETE FROM gamemods WHERE source = ?;");
  del.bind(1, source);
  del.step();

  Stmt ins(*db_, R"SQL(
    INSERT INTO gamemods(name, file_path, source, file_size, modified_at, status, repo_mod_id, last_scanned_at)
    VALUES(?, ?, ?, ?, ?, ?, ?, ?);
  )SQL");

  for (const auto& row : rows) {
    ins.bind(1, row.name);
    ins.bind(2, row.file_path);
    ins.bind(3, row.source);
    ins.bind(4, static_cast<sqlite3_int64>(row.file_size));
    if (!row.modified_at.empty()) {
      ins.bind(5, row.modified_at);
    } else {
      ins.bindNull(5);
    }
    ins.bind(6, row.status);
    bindOptionalInt(ins, 7, row.repo_mod_id);
    if (!row.last_scanned_at.empty()) {
      ins.bind(8, row.last_scanned_at);
    } else {
      ins.bindNull(8);
    }
    ins.step();
    ins.reset();
  }

  tx.commit();
}

std::optional<GameModRow> GameModDao::findByPath(const std::string& filePath) const {
  Stmt stmt(*db_, R"SQL(
    SELECT id, name, file_path, source, file_size, modified_at, status, repo_mod_id, last_scanned_at
    FROM gamemods
    WHERE file_path = ?;
  )SQL");
  stmt.bind(1, filePath);
  if (!stmt.step()) {
    return std::nullopt;
  }

  GameModRow row;
  row.id = stmt.getInt(0);
  row.name = stmt.getText(1);
  row.file_path = stmt.getText(2);
  row.source = stmt.getText(3);
  row.file_size = static_cast<std::uint64_t>(stmt.getInt64(4));
  if (!stmt.isNull(5)) {
    row.modified_at = stmt.getText(5);
  }
  row.status = stmt.getText(6);
  if (!stmt.isNull(7)) {
    row.repo_mod_id = stmt.getInt(7);
  }
  if (!stmt.isNull(8)) {
    row.last_scanned_at = stmt.getText(8);
  }
  return row;
}

void GameModDao::upsert(const GameModRow& row) {
  Db::Tx tx(*db_);
  Stmt stmt(*db_, R"SQL(
    INSERT INTO gamemods(name, file_path, source, file_size, modified_at, status, repo_mod_id, last_scanned_at)
    VALUES(?, ?, ?, ?, ?, ?, ?, ?)
    ON CONFLICT(file_path) DO UPDATE SET
      name = excluded.name,
      source = excluded.source,
      file_size = excluded.file_size,
      modified_at = excluded.modified_at,
      status = excluded.status,
      repo_mod_id = excluded.repo_mod_id,
      last_scanned_at = excluded.last_scanned_at;
  )SQL");
  stmt.bind(1, row.name);
  stmt.bind(2, row.file_path);
  stmt.bind(3, row.source);
  stmt.bind(4, static_cast<sqlite3_int64>(row.file_size));
  if (!row.modified_at.empty()) {
    stmt.bind(5, row.modified_at);
  } else {
    stmt.bindNull(5);
  }
  stmt.bind(6, row.status);
  bindOptionalInt(stmt, 7, row.repo_mod_id);
  if (!row.last_scanned_at.empty()) {
    stmt.bind(8, row.last_scanned_at);
  } else {
    stmt.bindNull(8);
  }
  stmt.step();
  tx.commit();
}

void GameModDao::removeByPaths(const std::string& source, const std::vector<std::string>& keepPaths) {
  Db::Tx tx(*db_);
  if (keepPaths.empty()) {
    Stmt del(*db_, "DELETE FROM gamemods WHERE source = ?;");
    del.bind(1, source);
    del.step();
    tx.commit();
    return;
  }

  std::string sql = "DELETE FROM gamemods WHERE source = ? AND file_path NOT IN (";
  for (size_t i = 0; i < keepPaths.size(); ++i) {
    if (i > 0) {
      sql.append(", ");
    }
    sql.append("?");
  }
  sql.push_back(')');

  Stmt del(*db_, sql);
  del.bind(1, source);
  for (size_t i = 0; i < keepPaths.size(); ++i) {
    del.bind(static_cast<int>(i + 2), keepPaths[i]);
  }
  del.step();
  tx.commit();
}

std::vector<GameModRow> GameModDao::listAll() const {
  Stmt stmt(*db_, R"SQL(
    SELECT id, name, file_path, source, file_size, modified_at, status, repo_mod_id, last_scanned_at
    FROM gamemods
    ORDER BY source, name COLLATE NOCASE;
  )SQL");
  std::vector<GameModRow> rows;
  while (stmt.step()) {
    GameModRow row;
    row.id = stmt.getInt(0);
    row.name = stmt.getText(1);
    row.file_path = stmt.getText(2);
    row.source = stmt.getText(3);
    row.file_size = static_cast<std::uint64_t>(stmt.getInt64(4));
    if (!stmt.isNull(5)) {
      row.modified_at = stmt.getText(5);
    }
    row.status = stmt.getText(6);
    if (!stmt.isNull(7)) {
      row.repo_mod_id = stmt.getInt(7);
    }
    if (!stmt.isNull(8)) {
      row.last_scanned_at = stmt.getText(8);
    }
    rows.push_back(std::move(row));
  }
  return rows;
}
