#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/repo/CategoryDao.h"
#include "core/repo/RepositoryDao.h"
#include "core/repo/TagDao.h"
#include "core/repo/ModRelationDao.h"
#include "core/repo/SelectionDao.h"

/**
 * @file RepositoryService.h
 * @brief Aggregates repository DAOs into business-oriented operations.
 * @note 面向上层业务的门面，所有操作默认在服务层内保证事务与幂等。
 */

struct TagDescriptor {
  std::string group; ///< TAG 组名称
  std::string tag;   ///< 组内具体条目
};

class RepositoryService {
public:
  explicit RepositoryService(std::shared_ptr<Db> db);

  /// 仓库中未被逻辑删除的 MOD 列表
  std::vector<ModRow> listVisible() const;
  /// 新建 MOD 并一次性绑定 TAG（组 + 条目）
  int createModWithTags(const ModRow& mod, const std::vector<TagDescriptor>& tags);
  /// 重建 MOD 的 TAG 集合（清空后重新绑定）
  void updateModTags(int modId, const std::vector<TagDescriptor>& tags);

  /// 分类维护接口
  std::vector<CategoryRow> listCategories() const;
  int createCategory(const std::string& name, std::optional<int> parentId);
  void updateCategory(int id, const std::string& name, std::optional<int> parentId);

  /// TAG 查询接口
  std::vector<TagGroupRow> listTagGroups() const;
  std::vector<TagWithGroupRow> listTags() const;
  std::vector<TagWithGroupRow> listTagsForMod(int modId) const;

  /// 模组关系（依赖 / 冲突等）维护
  std::vector<ModRelationRow> listRelationsForMod(int modId) const;
  int addRelation(const ModRelationRow& relation);
  void removeRelation(int relationId);
  void removeRelation(int aModId, int bModId, const std::string& type);

  /// 组合方案维护
  std::vector<SelectionRow> listSelections() const;
  std::vector<SelectionItemRow> listSelectionItems(int selectionId) const;
  int createSelection(const std::string& name, double budgetMb, const std::vector<SelectionItemRow>& items);
  void updateSelectionItems(int selectionId, const std::vector<SelectionItemRow>& items);
  void deleteSelection(int selectionId);

private:
  std::shared_ptr<Db> db_;
  std::unique_ptr<RepositoryDao> repoDao_;
  std::unique_ptr<CategoryDao> categoryDao_;
  std::unique_ptr<TagDao> tagDao_;
  std::unique_ptr<ModRelationDao> relationDao_;
  std::unique_ptr<SelectionDao> selectionDao_;
};
