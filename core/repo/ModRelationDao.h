#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file ModRelationDao.h
 * @brief DAO for mod_relations table operations.
 * @note 模组关系表 DAO，统一维护依赖/冲突等关系。
 */

struct ModRelationRow {
  int id;
  int a_mod_id;
  int b_mod_id;
  std::string type;//关系类型，requires, conflicts, homologous, custom_master, party
  std::optional<std::string> slot_key;
  std::optional<std::string> note;
};

class ModRelationDao {
public:
  explicit ModRelationDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /// 写入一条关系记录（含槽位信息）
  int insert(const ModRelationRow& row);
  /// 按主键删除关系
  void removeById(int id);
  /// 按端点和类型删除关系（用于成对解除）
  void removeBetween(int aModId, int bModId, const std::string& type);
  /// 查询与指定 MOD 相关的全部关系
  std::vector<ModRelationRow> listByMod(int modId) const;

private:
  std::shared_ptr<Db> db_;
};
