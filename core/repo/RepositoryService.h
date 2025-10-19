#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/repo/CategoryDao.h"
#include "core/repo/RepositoryDao.h"
#include "core/repo/TagDao.h"
#include "core/repo/ModRelationDao.h"
#include "core/repo/SavedSchemeDao.h"
#include "core/repo/FixedBundleDao.h"

/**
 * @file RepositoryService.h
 * @brief Aggregates repository DAOs into business-oriented operations.
 */

struct TagDescriptor {
  std::string group;
  std::string tag;
};

class RepositoryService {
public:
  explicit RepositoryService(std::shared_ptr<Db> db);

  std::vector<ModRow> listVisible() const;
  int createModWithTags(const ModRow& mod, const std::vector<TagDescriptor>& tags);
  void updateModTags(int modId, const std::vector<TagDescriptor>& tags);

  std::vector<CategoryRow> listCategories() const;
  int createCategory(const std::string& name, std::optional<int> parentId);
  void updateCategory(int id, const std::string& name, std::optional<int> parentId);

  std::vector<TagGroupRow> listTagGroups() const;
  std::vector<TagWithGroupRow> listTags() const;
  std::vector<TagWithGroupRow> listTagsForMod(int modId) const;

  std::vector<ModRelationRow> listRelationsForMod(int modId) const;
  int addRelation(const ModRelationRow& relation);
  void removeRelation(int relationId);
  void removeRelation(int aModId, int bModId, const std::string& type);

  std::vector<FixedBundleRow> listFixedBundles() const;
  std::vector<FixedBundleItemRow> listFixedBundleItems(int bundleId) const;
  int createFixedBundle(const std::string& name, const std::vector<int>& modIds, const std::optional<std::string>& note);
  void updateFixedBundle(int bundleId, const std::string& name, const std::vector<int>& modIds, const std::optional<std::string>& note);
  void deleteFixedBundle(int bundleId);

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
