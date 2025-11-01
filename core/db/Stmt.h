#pragma once
#include <string>
#include "core/db/Db.h"

/**
 * @file Stmt.h
 * @brief sqlite3 预处理语句的轻量级封装。
 */

class Stmt {
public:
  /**
   * @brief 构造函数，准备 SQL 语句。
   * @param db 数据库连接。
   * @param sql 要准备的 SQL 语句。
   * @throws DbError 如果准备失败。
   */
  Stmt(Db& db, const std::string& sql) : db_(db) {
    if (sqlite3_prepare_v2(db_.raw(), sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
      throw DbError("prepare failed: " + sql);
    }
  }

  /**
   * @brief 析构函数，终结语句句柄。
   */
  ~Stmt() {
    if (stmt_) {
      sqlite3_finalize(stmt_);
    }
  }

  /** @brief 绑定一个整型参数。 */
  void bind(int idx, int v) { sqlite3_bind_int(stmt_, idx, v); }
  /** @brief 绑定一个 64 位整型参数，适用于文件大小等大数值。 */
  void bind(int idx, sqlite3_int64 v) { sqlite3_bind_int64(stmt_, idx, v); }
  /** @brief 绑定一个浮点型参数。 */
  void bind(int idx, double v) { sqlite3_bind_double(stmt_, idx, v); }
  /** @brief 绑定一个字符串参数。 */
  void bind(int idx, const std::string& v) { sqlite3_bind_text(stmt_, idx, v.c_str(), -1, SQLITE_TRANSIENT); }
  /** @brief 绑定一个 NULL 参数。 */
  void bindNull(int idx) { sqlite3_bind_null(stmt_, idx); }

  /**
   * @brief 执行一步操作。
   * @return 如果有另一行数据，返回 true；如果执行完成，返回 false。
   * @throws DbError 如果执行失败。
   */
  bool step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw DbError("step failed");
  }

  /** @brief 获取一个整型列数据。 */
  int getInt(int col) const { return sqlite3_column_int(stmt_, col); }
  /** @brief 读取一个 64 位整型列数据。 */
  sqlite3_int64 getInt64(int col) const { return sqlite3_column_int64(stmt_, col); }
  /** @brief 获取一个浮点型列数据。 */
  double getDouble(int col) const { return sqlite3_column_double(stmt_, col); }
  /** @brief 获取一个字符串列数据。 */
  std::string getText(int col) const {
    auto* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, col));
    return p ? std::string(p) : std::string();
  }
  /** @brief 判断指定列是否为 NULL。 */
  bool isNull(int col) const { return sqlite3_column_type(stmt_, col) == SQLITE_NULL; }

  /**
   * @brief 重置语句以便重新执行，并清除所有绑定。
   */
  void reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
  }

  /** @brief 获取底层的 sqlite3_stmt* 句柄。 */
  sqlite3_stmt* raw() const { return stmt_; }

private:
  Db& db_;
  sqlite3_stmt* stmt_{nullptr};
};
