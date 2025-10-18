#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/repo/RepositoryDao.h"
#include "core/repo/CategoryDao.h"
#include "core/repo/TagDao.h"
#include "core/repo/ModRelationDao.h"
#include "core/repo/SelectionDao.h"

/**
 * @file RepositoryService.h
 * @brief Application-facing orchestration layer built on top of repository DAOs.
 * @note 仓库服务层，聚合各 DAO 提供业务级接口。
 */

class RepositoryService {
public:
  explicit RepositoryService(std::shared_ptr<Db> db);

  /// 查询仓库中可见的 MOD 列表
  std::vector<ModRow> listVisible() const;
  /// 创建新 MOD 并绑定 TAG（事务保障）
  int createModWithTags(const ModRow& mod, const std::vector<std::string>& tagNames);
  /// 重建某个 MOD 的 TAG 集合
  void updateModTags(int modId, const std::vector<std::string>& tagNames);

  /// 列出全量分类
  std::vector<CategoryRow> listCategories() const;
  /// 新建分类
  int createCategory(const std::string& name, std::optional<int> parentId);
  /// 更新分类信息
  void updateCategory(int id, const std::string& name, std::optional<int> parentId);

  /// 查询所有 TAG
  std::vector<TagRow> listTags() const;
  /// 查询 MOD 的 TAG
  std::vector<TagRow> listTagsForMod(int modId) const;

  /// 查询 MOD 关联的关系
  std::vector<ModRelationRow> listRelationsForMod(int modId) const;
  /// 新增关系
  int addRelation(const ModRelationRow& relation);
  /// 按 ID 删除关系
  void removeRelation(int relationId);
  /// 按端点删除关系
  void removeRelation(int aModId, int bModId, const std::string& type);

  /// 列出所有组合方案
  std::vector<SelectionRow> listSelections() const;
  /// 列出组合条目
  std::vector<SelectionItemRow> listSelectionItems(int selectionId) const;
  /// 创建组合并写入条目
  int createSelection(const std::string& name, double budgetMb, const std::vector<SelectionItemRow>& items);
  /// 覆盖更新组合条目
  void updateSelectionItems(int selectionId, const std::vector<SelectionItemRow>& items);
  /// 删除组合
  void deleteSelection(int selectionId);

private:
  std::shared_ptr<Db> db_;
  std::unique_ptr<RepositoryDao> repoDao_;
  std::unique_ptr<CategoryDao> categoryDao_;
  std::unique_ptr<TagDao> tagDao_;
  std::unique_ptr<ModRelationDao> relationDao_;
  std::unique_ptr<SelectionDao> selectionDao_;
};
