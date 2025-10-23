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
      stmt.getText(6),             // last_published_at
      stmt.getText(7),             // last_saved_at
      stmt.getText(8),             // status
      stmt.getText(9),             // source_platform
      stmt.getText(10),            // source_url
      stmt.getInt(11) != 0,        // is_deleted
      stmt.getText(12),            // cover_path
      stmt.getText(13),            // file_path
      stmt.getText(14),            // file_hash
      stmt.getDouble(15),          // size_mb
      stmt.getText(16),            // integrity
      stmt.getText(17),            // stability
      stmt.getText(18)             // acquisition_method
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
      last_published_at,
      last_saved_at,
      status,
      source_platform,
      source_url,
      is_deleted,
      cover_path,
      file_path,
      file_hash,
      size_mb,
      integrity,
      stability,
      acquisition_method
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
  )SQL");

  stmt.bind(1, row.name);
  bindOptionalText(stmt, 2, row.author);
  bindOptionalInt(stmt, 3, row.rating);
  bindOptionalInt(stmt, 4, row.category_id);
  bindOptionalText(stmt, 5, row.note);
  bindOptionalText(stmt, 6, row.last_published_at);
  bindOptionalText(stmt, 7, row.last_saved_at);
  stmt.bind(8, row.status.empty() ? "最新" : row.status);
  bindOptionalText(stmt, 9, row.source_platform);
  bindOptionalText(stmt, 10, row.source_url);
  stmt.bind(11, row.is_deleted ? 1 : 0);
  bindOptionalText(stmt, 12, row.cover_path);
  bindOptionalText(stmt, 13, row.file_path);
  bindOptionalText(stmt, 14, row.file_hash);
  stmt.bind(15, row.size_mb);
  bindOptionalText(stmt, 16, row.integrity);
  bindOptionalText(stmt, 17, row.stability);
  bindOptionalText(stmt, 18, row.acquisition_method);
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
      last_published_at = ?,
      last_saved_at = ?,
      status = ?,
      source_platform = ?,
      source_url = ?,
      cover_path = ?,
      file_path = ?,
      file_hash = ?,
      size_mb = ?,
      integrity = ?,
      stability = ?,
      acquisition_method = ?
    WHERE id = ?;
  )SQL");

  stmt.bind(1, row.name);
  bindOptionalText(stmt, 2, row.author);
  bindOptionalInt(stmt, 3, row.rating);
  bindOptionalInt(stmt, 4, row.category_id);
  bindOptionalText(stmt, 5, row.note);
  bindOptionalText(stmt, 6, row.last_published_at);
  bindOptionalText(stmt, 7, row.last_saved_at);
  stmt.bind(8, row.status.empty() ? "最新" : row.status);
  bindOptionalText(stmt, 9, row.source_platform);
  bindOptionalText(stmt, 10, row.source_url);
  bindOptionalText(stmt, 11, row.cover_path);
  bindOptionalText(stmt, 12, row.file_path);
  bindOptionalText(stmt, 13, row.file_hash);
  stmt.bind(14, row.size_mb);
  bindOptionalText(stmt, 15, row.integrity);
  bindOptionalText(stmt, 16, row.stability);
  bindOptionalText(stmt, 17, row.acquisition_method);
  stmt.bind(18, row.id);
  stmt.step();
}

void RepositoryDao::setDeleted(int id, bool deleted) {
  Stmt stmt(*db_, "UPDATE mods SET is_deleted = ? WHERE id = ?;");
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
      COALESCE(last_published_at, ''),
      COALESCE(last_saved_at, ''),
      COALESCE(status, '最新'),
      COALESCE(source_platform, ''),
      COALESCE(source_url, ''),
      is_deleted,
      COALESCE(cover_path, ''),
      COALESCE(file_path, ''),
      COALESCE(file_hash, ''),
      size_mb,
      COALESCE(integrity, ''),
      COALESCE(stability, ''),
      COALESCE(acquisition_method, '')
    FROM mods
    WHERE id = ?;
  )SQL");
  stmt.bind(1, id);
  if (!stmt.step()) {
    return std::nullopt;
  }
  return readRow(stmt);
}

std::optional<ModRow> RepositoryDao::findByFileHash(const std::string& fileHash) const {
  Stmt stmt(*db_, R"SQL(
    SELECT
      id,
      name,
      COALESCE(author, ''),
      COALESCE(rating, 0),
      COALESCE(category_id, 0),
      COALESCE(note, ''),
      COALESCE(last_published_at, ''),
      COALESCE(last_saved_at, ''),
      COALESCE(status, '最新'),
      COALESCE(source_platform, ''),
      COALESCE(source_url, ''),
      is_deleted,
      COALESCE(cover_path, ''),
      COALESCE(file_path, ''),
      COALESCE(file_hash, ''),
      size_mb,
      COALESCE(integrity, ''),
      COALESCE(stability, ''),
      COALESCE(acquisition_method, '')
    FROM mods
    WHERE file_hash = ?;
  )SQL");
  stmt.bind(1, fileHash);
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
      COALESCE(last_published_at, ''),
      COALESCE(last_saved_at, ''),
      COALESCE(status, '最新'),
      COALESCE(source_platform, ''),
      COALESCE(source_url, ''),
      is_deleted,
      COALESCE(cover_path, ''),
      COALESCE(file_path, ''),
      COALESCE(file_hash, ''),
      size_mb,
      COALESCE(integrity, ''),
      COALESCE(stability, ''),
      COALESCE(acquisition_method, '')
    FROM v_mods_visible
    ORDER BY name;
  )SQL");

  std::vector<ModRow> rows;
  while (stmt.step()) {
    rows.push_back(readRow(stmt));
  }
  return rows;
}
