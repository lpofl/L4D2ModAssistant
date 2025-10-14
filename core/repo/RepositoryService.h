
#pragma once
#include <memory>
#include <vector>
#include "core/repo/RepositoryDao.h"

class RepositoryService {
public:
  explicit RepositoryService(std::shared_ptr<Db> db) : dao_(std::make_unique<RepositoryDao>(db)) {}
  std::vector<ModRow> listVisible() { return dao_->listVisible(); }
private:
  std::unique_ptr<RepositoryDao> dao_;
};
