#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/repo/CategoryDao.h"
#include "core/repo/FixedBundleDao.h"
#include "core/repo/ModRelationDao.h"
#include "core/repo/RepositoryDao.h"
#include "core/repo/SavedSchemeDao.h"
#include "core/repo/TagDao.h"

/**
 * @file RepositoryService.h
 * @brief Aggregates repository DAOs into business-oriented operations.
 */

/// Lightweight descriptor used by the UI layer when editing tags.
struct TagDescriptor {
  std::string group;
  std::string tag;
};

class RepositoryService {
public:
  explicit RepositoryService(std::shared_ptr<Db> db);

  /// Query visible (not deleted) mods.
  std::vector<ModRow> listVisible() const;
  /// Query all mods, optionally including logically deleted ones.
  std::vector<ModRow> listAll(bool includeDeleted = false) const;
  /// Lookup a mod by id.
  std::optional<ModRow> findMod(int modId) const;
  /// Create mod and bind tags atomically.
  int createModWithTags(const ModRow& mod, const std::vector<TagDescriptor>& tags);
  /// Update mod fields and refresh tag bindings.
  void updateModWithTags(const ModRow& mod, const std::vector<TagDescriptor>& tags);
  /// Replace only the tag bindings (mod data unchanged).
  void updateModTags(int modId, const std::vector<TagDescriptor>& tags);
  /// Toggle logical deletion flag.
  void setModDeleted(int modId, bool deleted);
  /// Permanently delete all logically deleted mods.
  void clearDeletedMods();

  /// Category management.
  std::vector<CategoryRow> listCategories() const;
  int createCategory(const std::string& name, std::optional<int> parentId);
  void updateCategory(int id, const std::string& name, std::optional<int> parentId,
                      std::optional<int> priority = std::nullopt);
  void deleteCategory(int id);
  void swapCategoryPriority(int firstId, int secondId);

  /// Tag queries.
  std::vector<TagGroupRow> listTagGroups() const;
  int createTagGroup(const std::string& name);
  void renameTagGroup(int groupId, const std::string& name);
  bool deleteTagGroup(int groupId);
  std::vector<TagWithGroupRow> listTags() const;
  std::vector<TagRow> listTagsInGroup(int groupId) const;
  int createTag(int groupId, const std::string& name);
  void renameTag(int tagId, const std::string& name);
  bool deleteTag(int tagId);
  std::vector<TagWithGroupRow> listTagsForMod(int modId) const;

  /// Relation maintenance.
  std::vector<ModRelationRow> listRelationsForMod(int modId) const;
  int addRelation(const ModRelationRow& relation);
  void removeRelation(int relationId);
  void removeRelation(int aModId, int bModId, const std::string& type);
  /// 批量替换指定 MOD 的关系记录，内部以事务方式先删后写。
  void replaceRelationsForMod(int modId, const std::vector<ModRelationRow>& relations);

  /// Fixed bundle management.
  std::vector<FixedBundleRow> listFixedBundles() const;
  std::vector<FixedBundleItemRow> listFixedBundleItems(int bundleId) const;
  int createFixedBundle(const std::string& name, const std::vector<int>& modIds, const std::optional<std::string>& note);
  void updateFixedBundle(int bundleId, const std::string& name, const std::vector<int>& modIds, const std::optional<std::string>& note);
  void deleteFixedBundle(int bundleId);

  /// Saved scheme management.
  std::vector<SavedSchemeRow> listSavedSchemes() const;
  std::vector<SavedSchemeItemRow> listSavedSchemeItems(int schemeId) const;
  int createSavedScheme(const std::string& name, double budgetMb, const std::vector<SavedSchemeItemRow>& items);
  void updateSavedSchemeItems(int schemeId, const std::vector<SavedSchemeItemRow>& items);
  void deleteSavedScheme(int schemeId);

private:
  std::shared_ptr<Db> db_;
  std::unique_ptr<RepositoryDao> repoDao_;
  std::unique_ptr<CategoryDao> categoryDao_;
  std::unique_ptr<TagDao> tagDao_;
  std::unique_ptr<ModRelationDao> relationDao_;
  std::unique_ptr<SavedSchemeDao> savedSchemeDao_;
  std::unique_ptr<FixedBundleDao> fixedBundleDao_;
};
