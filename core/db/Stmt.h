
#pragma once
#include "core/db/Db.h"

/**
 * @file Stmt.h
 * @brief 轻量的 sqlite3 语句封装，负责 prepare/bind/step/reset 生命周期管理。
 */

/**
 * @brief 预编译 SQL 语句的 RAII 包装。
 */
class Stmt {
public:
  /**
   * @brief 构造函数，会执行 sqlite3_prepare_v2。
   * @param db 关联的数据库实例。
   * @param sql 待预编译的 SQL 语句。
   * @throws DbError 当预编译失败时抛出。
   */
  Stmt(Db& db, const std::string& sql) : db_(db) {
    if (sqlite3_prepare_v2(db_.raw(), sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK)
      throw DbError("prepare failed: " + sql);
  }
  ~Stmt() { if (stmt_) sqlite3_finalize(stmt_); }

  /** @brief 绑定 int 参数。 */
  void bind(int idx, int v) { sqlite3_bind_int(stmt_, idx, v); }
  /** @brief 绑定 double 参数。 */
  void bind(int idx, double v) { sqlite3_bind_double(stmt_, idx, v); }
  /** @brief 绑定 text 参数。 */
  void bind(int idx, const std::string& v) { sqlite3_bind_text(stmt_, idx, v.c_str(), -1, SQLITE_TRANSIENT); }
  /** @brief 绑定 NULL 参数。 */
  void bindNull(int idx){ sqlite3_bind_null(stmt_, idx); }

  /**
   * @brief 执行一步，返回是否产生数据行。
   * @return true 表示获得一行数据；false 表示执行完成无更多数据。
   * @throws DbError 当底层 step 返回错误码。
   */
  bool step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw DbError("step failed");
  }

  /** @brief 读取整型列值。 */
  int getInt(int col) const { return sqlite3_column_int(stmt_, col); }
  /** @brief 读取浮点列值。 */
  double getDouble(int col) const { return sqlite3_column_double(stmt_, col); }
  /** @brief 读取文本列值（为空指针时返回空字符串）。 */
  std::string getText(int col) const {
    auto* p = (const char*)sqlite3_column_text(stmt_, col);
    return p ? std::string(p) : std::string();
  }

  /** @brief 重置语句并清除绑定参数。 */
  void reset() { sqlite3_reset(stmt_); sqlite3_clear_bindings(stmt_); }
  /** @brief 暴露底层 sqlite3_stmt*。 */
  sqlite3_stmt* raw() const { return stmt_; }

private:
  Db& db_;                ///< 关联数据库。
  sqlite3_stmt* stmt_{nullptr}; ///< 底层语句句柄。
};
