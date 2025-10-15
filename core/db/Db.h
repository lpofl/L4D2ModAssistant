
#pragma once //防止头文件被重复包含
#include <sqlite3.h> //包含sqlite3.h头文件
#include <string>
#include <stdexcept>

/**
 * @file Db.h
 * @brief sqlite3 数据库轻量封装，包含事务与执行工具。
 */

/**
 * @brief 数据库错误类型。
 */
class DbError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

/**
 * @brief sqlite3 数据库封装。
 */
class Db {
public:
  /**
   * @brief 构造并打开数据库，同时设置推荐 Pragmas。
   * @param path 数据库文件路径。
   */
  explicit Db(const std::string& path) { open(path); initPragmas(); }
  ~Db() { if (db_) sqlite3_close(db_); }

  Db(const Db&) = delete; Db& operator=(const Db&) = delete;//禁止拷贝构造和赋值
  Db(Db&& o) noexcept : db_(o.db_) { o.db_ = nullptr; }//移动构造函数

  /** @brief 获取底层 sqlite3*。 */
  sqlite3* raw() const { return db_; }

  /**
   * @brief 直接执行 SQL（无结果集）。
   * @param sql 要执行的 SQL 脚本。
   * @throws DbError 当执行失败。
   */
  void exec(const std::string& sql);

  /**
   * @brief 简单事务 RAII 对象。
   */
  class Tx {
  public:
    /** @brief 开始一个 IMMEDIATE 事务。 */
    explicit Tx(Db& d) : db_(d) { db_.exec("BEGIN IMMEDIATE;"); committed_ = false; }
    /** @brief 析构时若未提交则回滚。 */
    ~Tx() { if (!committed_) try { db_.exec("ROLLBACK;"); } catch(...) {} }
    /** @brief 提交事务。 */
    void commit() { db_.exec("COMMIT;"); committed_ = true; }
  private:
    Db& db_; ///< 关联数据库。
    bool committed_{false}; ///< 提交标志。
  };

private:
  /** @brief 打开数据库文件。 */
  void open(const std::string& path);
  /** @brief 设置推荐的 Pragmas。 */
  void initPragmas();
  sqlite3* db_{nullptr}; ///< 底层连接句柄。
};
