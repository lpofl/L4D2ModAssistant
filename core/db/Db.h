
#pragma once
#include <sqlite3.h>
#include <string>
#include <stdexcept>

class DbError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class Db {
public:
  explicit Db(const std::string& path) { open(path); initPragmas(); }
  ~Db() { if (db_) sqlite3_close(db_); }

  Db(const Db&) = delete; Db& operator=(const Db&) = delete;
  Db(Db&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }

  sqlite3* raw() const { return db_; }
  void exec(const std::string& sql);

  class Tx {
  public:
    explicit Tx(Db& d) : db_(d) { db_.exec("BEGIN IMMEDIATE;"); committed_ = false; }
    ~Tx() { if (!committed_) try { db_.exec("ROLLBACK;"); } catch(...) {} }
    void commit() { db_.exec("COMMIT;"); committed_ = true; }
  private:
    Db& db_; bool committed_{false};
  };

private:
  void open(const std::string& path);
  void initPragmas();
  sqlite3* db_{nullptr};
};
