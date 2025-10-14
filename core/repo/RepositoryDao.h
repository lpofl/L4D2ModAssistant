
#pragma once
#include <vector>
#include <string>
#include <memory>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

struct ModRow {
  int id; std::string name; int rating; int category_id;
  double size_mb; bool is_deleted; std::string file_path; std::string file_hash;
};

class RepositoryDao {
public:
  explicit RepositoryDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}
  int insertMod(const ModRow& m);
  std::vector<ModRow> listVisible();
private:
  std::shared_ptr<Db> db_;
};
