
#pragma once
#include <memory>
#include <vector>
#include "core/repo/RepositoryDao.h"

/**
 * @file RepositoryService.h
 * @brief 仓库领域服务，封装对 DAO 的调用与后续可扩展的业务逻辑。
 */

/**
 * @brief 仓库服务层。
 *
 * 负责对外提供查询与操作接口，当前 MVP 仅转发至 RepositoryDao。
 */
class RepositoryService {
public:
  /**
   * @brief 构造服务实例。
   * @param db 已打开的数据库句柄共享指针。
   */
  explicit RepositoryService(std::shared_ptr<Db> db) : dao_(std::make_unique<RepositoryDao>(db)) {}

  /**
   * @brief 列出未被标记删除的 Mod 列表（对用户可见）。
   * @return Mod 行集合。
   */
  std::vector<ModRow> listVisible() { return dao_->listVisible(); }
private:
  /** @brief 数据访问对象。 */
  std::unique_ptr<RepositoryDao> dao_;
};
