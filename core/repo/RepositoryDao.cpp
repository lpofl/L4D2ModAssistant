#include "core/repo/RepositoryDao.h"

namespace {

void bindOptionalText(Stmt& stmt, int index, const std::string& value) {
  if (value.empty()) {
    stmt.bindNull(index);
  } else {
    stmt.bind(index, value);
  }
}

void bindOptionalInt(Stmt& stmt, int index, int value) {
  if (value > 0) {
    stmt.bind(index, value);
  } else {
    stmt.bindNull(index);
  }
}

ModRow readRow(Stmt& stmt) {
  return ModRow{
      stmt.getInt(0),              // id
      stmt.getText(1),             // name
      stmt.getText(2),             // author
      stmt.getInt(3),              // rating (0 if null)
      stmt.getInt(4),              // category_id (0 if null)
      stmt.getText(5),             // note
      stmt.getText(6),             // published_at
      stmt.getText(7),             // source_platform
      stmt.getText(8),             // source_url
      stmt.getInt(9) != 0,         // is_deleted
      stmt.getText(10),            // cover_path
      stmt.getText(11),            // file_path
      stmt.getText(12),            // file_hash
      stmt.getDouble(13),          // size_mb
      stmt.getText(14),            // created_at
      stmt.getText(15)             // updated_at
  };
}

} // namespace

int RepositoryDao::insertMod(const ModRow& row) {
  Stmt stmt(*db_, R"SQL(
    INSERT INTO mods(
      name,
      author,
      rating,
      category_id,
      note,
      published_at,
      source_platform,
      source_url,
      is_deleted,
      cover_path,
      file_path,
      file_hash,
      size_mb
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
  )SQL");

  stmt.bind(1, row.name);
  bindOptionalText(stmt, 2, row.author);
  bindOptionalInt(stmt, 3, row.rating);
  bindOptionalInt(stmt, 4, row.category_id);
  bindOptionalText(stmt, 5, row.note);
  bindOptionalText(stmt, 6, row.published_at);
      bindOptionalText(stmt, 7, row.source_platform);
  bindOptionalText(stmt, 8, row.source_url);
  stmt.bind(9, row.is_deleted ? 1 : 0);
  bindOptionalText(stmt, 10, row.cover_path);
  bindOptionalText(stmt, 11, row.file_path);
  bindOptionalText(stmt, 12, row.file_hash);
  stmt.bind(13, row.size_mb);
  stmt.step();
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void RepositoryDao::updateMod(const ModRow& row) {
  Stmt stmt(*db_, R"SQL(
    UPDATE mods SET
      name = ?,
      author = ?,
      rating = ?,
      category_id = ?,
      note = ?,
      published_at = ?,
      source_platform = ?,
      source_url = ?,
      cover_path = ?,
      file_path = ?,
      file_hash = ?,
      size_mb = ?,
      updated_at = datetime('now')
    WHERE id = ?;
  )SQL");

  stmt.bind(1, row.name);
  bindOptionalText(stmt, 2, row.author);
  bindOptionalInt(stmt, 3, row.rating);
  bindOptionalInt(stmt, 4, row.category_id);
  bindOptionalText(stmt, 5, row.note);
  bindOptionalText(stmt, 6, row.published_at);
  bindOptionalText(stmt, 7, row.source_platform);
  bindOptionalText(stmt, 8, row.source_url);
  bindOptionalText(stmt, 9, row.cover_path);
  bindOptionalText(stmt, 10, row.file_path);
  bindOptionalText(stmt, 11, row.file_hash);
  stmt.bind(12, row.size_mb);
  stmt.bind(13, row.id);
  stmt.step();
}

void RepositoryDao::setDeleted(int id, bool deleted) {
  Stmt stmt(*db_, "UPDATE mods SET is_deleted = ?, updated_at = datetime('now') WHERE id = ?;");
  stmt.bind(1, deleted ? 1 : 0);
  stmt.bind(2, id);
  stmt.step();
}

std::optional<ModRow> RepositoryDao::findById(int id) const {
  Stmt stmt(*db_, R"SQL(
    SELECT
      id,
      name,
      COALESCE(author, ''),
      COALESCE(rating, 0),
      COALESCE(category_id, 0),
      COALESCE(note, ''),
      COALESCE(published_at, ''),
      COALESCE(source_platform, ''),
      COALESCE(source_url, ''),
      is_deleted,
      COALESCE(cover_path, ''),
      COALESCE(file_path, ''),
      COALESCE(file_hash, ''),
      size_mb,
      COALESCE(created_at, ''),
      COALESCE(updated_at, '')
    FROM mods
    WHERE id = ?;
  )SQL");
  stmt.bind(1, id);
  if (!stmt.step()) {
    return std::nullopt;
  }
  return readRow(stmt);
}

std::vector<ModRow> RepositoryDao::listVisible() const {
  Stmt stmt(*db_, R"SQL(
    SELECT
      id,
      name,
      COALESCE(author, ''),
      COALESCE(rating, 0),
      COALESCE(category_id, 0),
      COALESCE(note, ''),
      COALESCE(published_at, ''),
      COALESCE(source_platform, ''),
      COALESCE(source_url, ''),
      is_deleted,
      COALESCE(cover_path, ''),
      COALESCE(file_path, ''),
      COALESCE(file_hash, ''),
      size_mb,
      COALESCE(created_at, ''),
      COALESCE(updated_at, '')
    FROM v_mods_visible
    ORDER BY name;
  )SQL");

  std::vector<ModRow> rows;
  while (stmt.step()) {
    rows.push_back(readRow(stmt));
  }
  return rows;
}
