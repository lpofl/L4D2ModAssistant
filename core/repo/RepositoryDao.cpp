#include "core/repo/RepositoryDao.h"

/**
 * @file RepositoryDao.cpp
 * @brief 实现了 RepositoryDao 类中定义的方法。
 */

namespace {

/**
 * @brief 绑定一个可选的文本字段。如果字符串为空，则绑定NULL。
 * @param stmt SQLite 语句对象。
 * @param index 参数索引。
 * @param value 要绑定的字符串。
 */
void bindOptionalText(Stmt& stmt, int index, const std::string& value) {
  if (value.empty()) {
    stmt.bindNull(index);
  } else {
    stmt.bind(index, value);
  }
}

/**
 * @brief 绑定一个可选的整数字段。如果值小于等于0，则绑定NULL。
 * @param stmt SQLite 语句对象。
 * @param index 参数索引。
 * @param value 要绑定的整数。
 */
void bindOptionalInt(Stmt& stmt, int index, int value) {
  if (value > 0) {
    stmt.bind(index, value);
  } else {
    stmt.bindNull(index);
  }
}

/**
 * @brief 从一个已执行step()的查询语句中读取一行数据并填充到 ModRow 结构体。
 * @details 此函数假设查询的 SELECT 子句列顺序与 ModRow 成员顺序匹配。
 *          它依赖于查询中的 COALESCE 来处理 NULL 值，因此不在此处进行 null 检查。
 * @param stmt 已执行 step() 的 SQLite 语句对象。
 * @return 填充了数据的 ModRow 对象。
 */
ModRow readRow(Stmt& stmt) {
  return ModRow{
      stmt.getInt(0),              // id
      stmt.getText(1),             // name
      stmt.getText(2),             // author
      stmt.getInt(3),              // rating
      stmt.getInt(4),              // category_id
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
  // 准备 SQL INSERT 语句
  Stmt stmt(*db_, R"SQL(
    INSERT INTO mods(
      name, author, rating, category_id, note, last_published_at, last_saved_at,
      status, source_platform, source_url, is_deleted, cover_path, file_path,
      file_hash, size_mb, integrity, stability, acquisition_method
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
  )SQL");

  // 依次绑定 ModRow 中的所有字段到语句中
  stmt.bind(1, row.name);
  bindOptionalText(stmt, 2, row.author);
  bindOptionalInt(stmt, 3, row.rating);
  bindOptionalInt(stmt, 4, row.category_id);
  bindOptionalText(stmt, 5, row.note);
  bindOptionalText(stmt, 6, row.last_published_at);
  bindOptionalText(stmt, 7, row.last_saved_at);
  stmt.bind(8, row.status.empty() ? "最新" : row.status); // 确保 status 不为空
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
  // 准备 SQL UPDATE 语句
  Stmt stmt(*db_, R"SQL(
    UPDATE mods SET
      name = ?, author = ?, rating = ?, category_id = ?, note = ?, last_published_at = ?,
      last_saved_at = ?, status = ?, source_platform = ?, source_url = ?, cover_path = ?,
      file_path = ?, file_hash = ?, size_mb = ?, integrity = ?, stability = ?,
      acquisition_method = ?
    WHERE id = ?;
  )SQL");

  // 依次绑定 ModRow 中的所有字段到语句中
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
  stmt.bind(18, row.id); // 绑定 WHERE 子句的 ID
  
  stmt.step();
}

void RepositoryDao::setDeleted(int id, bool deleted) {
  // 更新指定 ID 的 MOD 的 is_deleted 标志
  Stmt stmt(*db_, "UPDATE mods SET is_deleted = ? WHERE id = ?;");
  stmt.bind(1, deleted ? 1 : 0);
  stmt.bind(2, id);
  stmt.step();
}

void RepositoryDao::deleteDeletedMods() {
  // 物理删除所有被标记为 is_deleted = 1 的 MOD
  Stmt stmt(*db_, "DELETE FROM mods WHERE is_deleted = 1;");
  stmt.step();
}

std::optional<ModRow> RepositoryDao::findById(int id) const {
  // 使用 COALESCE 将 NULL 值转换为空字符串或0，以便 readRow 函数处理
  Stmt stmt(*db_, R"SQL(
    SELECT id, name, COALESCE(author, ''), COALESCE(rating, 0), COALESCE(category_id, 0),
           COALESCE(note, ''), COALESCE(last_published_at, ''), COALESCE(last_saved_at, ''),
           COALESCE(status, '最新'), COALESCE(source_platform, ''), COALESCE(source_url, ''),
           is_deleted, COALESCE(cover_path, ''), COALESCE(file_path, ''),
           COALESCE(file_hash, ''), size_mb, COALESCE(integrity, ''),
           COALESCE(stability, ''), COALESCE(acquisition_method, '')
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
  // 同样使用 COALESCE 处理 NULL 值
  Stmt stmt(*db_, R"SQL(
    SELECT id, name, COALESCE(author, ''), COALESCE(rating, 0), COALESCE(category_id, 0),
           COALESCE(note, ''), COALESCE(last_published_at, ''), COALESCE(last_saved_at, ''),
           COALESCE(status, '最新'), COALESCE(source_platform, ''), COALESCE(source_url, ''),
           is_deleted, COALESCE(cover_path, ''), COALESCE(file_path, ''),
           COALESCE(file_hash, ''), size_mb, COALESCE(integrity, ''),
           COALESCE(stability, ''), COALESCE(acquisition_method, '')
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
  // 从 v_mods_visible 视图查询，该视图已预先过滤掉 is_deleted = 1 的记录
  Stmt stmt(*db_, R"SQL(
    SELECT id, name, COALESCE(author, ''), COALESCE(rating, 0), COALESCE(category_id, 0),
           COALESCE(note, ''), COALESCE(last_published_at, ''), COALESCE(last_saved_at, ''),
           COALESCE(status, '最新'), COALESCE(source_platform, ''), COALESCE(source_url, ''),
           is_deleted, COALESCE(cover_path, ''), COALESCE(file_path, ''),
           COALESCE(file_hash, ''), size_mb, COALESCE(integrity, ''),
           COALESCE(stability, ''), COALESCE(acquisition_method, '')
    FROM v_mods_visible
    ORDER BY name;
  )SQL");

  std::vector<ModRow> rows;
  while (stmt.step()) {
    rows.push_back(readRow(stmt));
  }
  return rows;
}

std::vector<ModRow> RepositoryDao::listAll(bool includeDeleted) const {
  // 动态构建 SQL 查询
  std::string sql = R"SQL(
    SELECT id, name, COALESCE(author, ''), COALESCE(rating, 0), COALESCE(category_id, 0),
           COALESCE(note, ''), COALESCE(last_published_at, ''), COALESCE(last_saved_at, ''),
           COALESCE(status, '最新'), COALESCE(source_platform, ''), COALESCE(source_url, ''),
           is_deleted, COALESCE(cover_path, ''), COALESCE(file_path, ''),
           COALESCE(file_hash, ''), size_mb, COALESCE(integrity, ''),
           COALESCE(stability, ''), COALESCE(acquisition_method, '')
    FROM mods
  )SQL";

  // 如果不包含已删除的，则添加 WHERE 子句
  if (!includeDeleted) {
    sql += " WHERE is_deleted = 0";
  }
  sql += " ORDER BY name;";

  Stmt stmt(*db_, sql);

  std::vector<ModRow> rows;
  while (stmt.step()) {
    rows.push_back(readRow(stmt));
  }
  return rows;
}