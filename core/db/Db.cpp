
#include "core/db/Db.h"

void Db::open(const std::string& path) {
  if (sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
    throw DbError("failed to open db: " + path);
  }
}

void Db::initPragmas() {
  exec("PRAGMA foreign_keys = ON;");
  exec("PRAGMA journal_mode = WAL;");
  exec("PRAGMA synchronous = NORMAL;");
  exec("PRAGMA temp_store = MEMORY;");
  exec("PRAGMA cache_size = -8000;");
}

void Db::exec(const std::string& sql) {
  char* err = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "unknown";
    sqlite3_free(err);
    throw DbError("sqlite exec error: " + msg + " | SQL: " + sql);
  }
}
