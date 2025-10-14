
#pragma once
#include "core/db/Db.h"

class Stmt {
public:
  Stmt(Db& db, const std::string& sql) : db_(db) {
    if (sqlite3_prepare_v2(db_.raw(), sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK)
      throw DbError("prepare failed: " + sql);
  }
  ~Stmt() { if (stmt_) sqlite3_finalize(stmt_); }

  void bind(int idx, int v) { sqlite3_bind_int(stmt_, idx, v); }
  void bind(int idx, double v) { sqlite3_bind_double(stmt_, idx, v); }
  void bind(int idx, const std::string& v) { sqlite3_bind_text(stmt_, idx, v.c_str(), -1, SQLITE_TRANSIENT); }
  void bindNull(int idx){ sqlite3_bind_null(stmt_, idx); }

  bool step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw DbError("step failed");
  }

  int getInt(int col) const { return sqlite3_column_int(stmt_, col); }
  double getDouble(int col) const { return sqlite3_column_double(stmt_, col); }
  std::string getText(int col) const {
    auto* p = (const char*)sqlite3_column_text(stmt_, col);
    return p ? std::string(p) : std::string();
  }

  void reset() { sqlite3_reset(stmt_); sqlite3_clear_bindings(stmt_); }
  sqlite3_stmt* raw() const { return stmt_; }

private:
  Db& db_; sqlite3_stmt* stmt_{nullptr};
};
