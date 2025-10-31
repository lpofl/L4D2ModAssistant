#include "core/repo/GameModDao.h"

/**
 * @file GameModDao.cpp
 * @brief 实现了 GameModDao 类中定义的方法。
 */

namespace {

/**
 * @brief 将一个 std::optional<int> 类型的值绑定到 SQLite 语句的参数上。
 * @param stmt SQLite 语句对象。
 * @param index 参数的索引（从1开始）。
 * @param value 要绑定的可选整数值。如果值存在，则绑定该整数；否则，绑定 NULL。
 */
inline void bindOptionalInt(Stmt& stmt, int index, const std::optional<int>& value) {
  if (value.has_value()) {
    stmt.bind(index, value.value());
  } else {
    stmt.bindNull(index);
  }
}

}  // namespace

void GameModDao::replaceForSource(const std::string& source, const std::vector<GameModRow>& rows) {
  // 在单个事务中执行“先删除后插入”的操作，确保数据一致性
  Db::Tx tx(*db_);
  
  // 步骤1: 删除该来源（source）下的所有旧记录
  Stmt del(*db_, "DELETE FROM gamemods WHERE source = ?;");
  del.bind(1, source);
  del.step();

  // 步骤2: 准备插入新记录的语句
  Stmt ins(*db_, R"SQL(
    INSERT INTO gamemods(name, file_path, source, file_size, modified_at, status, repo_mod_id, last_scanned_at)
    VALUES(?, ?, ?, ?, ?, ?, ?, ?);
  )SQL");

  // 步骤3: 遍历并插入所有新行
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
    ins.reset(); // 重置语句以便下次循环使用
  }

  // 提交事务
  tx.commit();
}

std::optional<GameModRow> GameModDao::findByPath(const std::string& filePath) const {
  Stmt stmt(*db_, R"SQL(
    SELECT id, name, file_path, source, file_size, modified_at, status, repo_mod_id, last_scanned_at
    FROM gamemods
    WHERE file_path = ?;
  )SQL");
  stmt.bind(1, filePath);
  
  // 如果没有查询到结果，则返回空
  if (!stmt.step()) {
    return std::nullopt;
  }

  // 从查询结果中填充 GameModRow 结构
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
  // 使用 "INSERT ... ON CONFLICT DO UPDATE" (即 "upsert") 语法
  // 如果 file_path 已存在，则更新现有记录；否则，插入新记录。
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
  
  // 绑定所有字段的值
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
  
  // 如果没有要保留的路径，则直接删除该来源下的所有记录
  if (keepPaths.empty()) {
    Stmt del(*db_, "DELETE FROM gamemods WHERE source = ?;");
    del.bind(1, source);
    del.step();
    tx.commit();
    return;
  }

  // 动态构建 SQL 查询语句，形式为 "DELETE ... WHERE source = ? AND file_path NOT IN (?, ?, ...)"
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
  // 绑定 NOT IN 子句中的所有路径
  for (size_t i = 0; i < keepPaths.size(); ++i) {
    del.bind(static_cast<int>(i + 2), keepPaths[i]);
  }
  del.step();
  tx.commit();
}

std::vector<GameModRow> GameModDao::listAll() const {
  // 查询所有记录，并按来源和名称（不区分大小写）排序
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