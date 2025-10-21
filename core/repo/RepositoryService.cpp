#include "core/repo/RepositoryService.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace {

std::string trimCopy(std::string_view text) {
  auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
  auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) { return std::isspace(c); }).base();
  if (begin >= end) return {};
  return std::string(begin, end);
}

std::vector<TagDescriptor> normalizeDescriptors(const std::vector<TagDescriptor>& tags) {
  std::vector<TagDescriptor> normalized;
  normalized.reserve(tags.size());
  std::unordered_set<std::string> dedup;

  for (const auto& tag : tags) {
    std::string group = trimCopy(tag.group);
    std::string value = trimCopy(tag.tag);
    if (group.empty() || value.empty()) continue;
    std::string key = group + '\0' + value;
    if (dedup.insert(key).second) {
      normalized.push_back({std::move(group), std::move(value)});
    }
  }
  return normalized;
}

std::vector<int> ensureTagIds(TagDao& tagDao, const std::vector<TagDescriptor>& tags) {
  auto normalized = normalizeDescriptors(tags);
  std::vector<int> ids;
  ids.reserve(normalized.size());
  for (const auto& tag : normalized) {
    int groupId = tagDao.ensureGroupId(tag.group);
    ids.push_back(tagDao.ensureTagId(groupId, tag.tag));
  }
  return ids;
}

void replaceModTags(TagDao& tagDao, int modId, const std::vector<int>& tagIds) {
  tagDao.clearTagsForMod(modId);
  for (int tagId : tagIds) {
    tagDao.addTagToMod(modId, tagId);
  }
}

} // namespace

RepositoryService::RepositoryService(std::shared_ptr<Db> db)
    : db_(std::move(db)),
      repoDao_(std::make_unique<RepositoryDao>(db_)),
      categoryDao_(std::make_unique<CategoryDao>(db_)),
      tagDao_(std::make_unique<TagDao>(db_)),
      relationDao_(std::make_unique<ModRelationDao>(db_)),
      savedSchemeDao_(std::make_unique<SavedSchemeDao>(db_)),
      fixedBundleDao_(std::make_unique<FixedBundleDao>(db_)) {}

std::vector<ModRow> RepositoryService::listVisible() const {
  return repoDao_->listVisible();
}

std::optional<ModRow> RepositoryService::findMod(int modId) const {
  return repoDao_->findById(modId);
}

int RepositoryService::createModWithTags(const ModRow& mod, const std::vector<TagDescriptor>& tags) {
  Db::Tx tx(*db_);
  const int modId = repoDao_->insertMod(mod);
  const auto tagIds = ensureTagIds(*tagDao_, tags);
  replaceModTags(*tagDao_, modId, tagIds);
  tx.commit();
  return modId;
}

void RepositoryService::updateModWithTags(const ModRow& mod, const std::vector<TagDescriptor>& tags) {
  if (mod.id <= 0) {
    throw DbError("updateModWithTags requires a valid mod id");
  }
  Db::Tx tx(*db_);
  repoDao_->updateMod(mod);
  const auto tagIds = ensureTagIds(*tagDao_, tags);
  replaceModTags(*tagDao_, mod.id, tagIds);
  tx.commit();
}

void RepositoryService::updateModTags(int modId, const std::vector<TagDescriptor>& tags) {
  Db::Tx tx(*db_);
  const auto tagIds = ensureTagIds(*tagDao_, tags);
  replaceModTags(*tagDao_, modId, tagIds);
  tx.commit();
}

void RepositoryService::setModDeleted(int modId, bool deleted) {
  repoDao_->setDeleted(modId, deleted);
}

std::vector<CategoryRow> RepositoryService::listCategories() const {
  return categoryDao_->listAll();
}

int RepositoryService::createCategory(const std::string& name, std::optional<int> parentId) {
  return categoryDao_->insert(name, parentId);
}

void RepositoryService::updateCategory(int id, const std::string& name, std::optional<int> parentId) {
  categoryDao_->update(id, name, parentId);
}

std::vector<TagGroupRow> RepositoryService::listTagGroups() const {
  return tagDao_->listGroups();
}

std::vector<TagWithGroupRow> RepositoryService::listTags() const {
  return tagDao_->listAllWithGroup();
}

std::vector<TagWithGroupRow> RepositoryService::listTagsForMod(int modId) const {
  return tagDao_->listByMod(modId);
}

std::vector<ModRelationRow> RepositoryService::listRelationsForMod(int modId) const {
  return relationDao_->listByMod(modId);
}

int RepositoryService::addRelation(const ModRelationRow& relation) {
  if (relation.a_mod_id == relation.b_mod_id) {
    throw DbError("relation endpoints cannot be the same mod");
  }
  return relationDao_->insert(relation);
}

void RepositoryService::removeRelation(int relationId) {
  relationDao_->removeById(relationId);
}

void RepositoryService::removeRelation(int aModId, int bModId, const std::string& type) {
  relationDao_->removeBetween(aModId, bModId, type);
}

std::vector<FixedBundleRow> RepositoryService::listFixedBundles() const {
  return fixedBundleDao_->listBundles();
}

std::vector<FixedBundleItemRow> RepositoryService::listFixedBundleItems(int bundleId) const {
  return fixedBundleDao_->listItems(bundleId);
}

int RepositoryService::createFixedBundle(const std::string& name, const std::vector<int>& modIds,
                                         const std::optional<std::string>& note) {
  Db::Tx tx(*db_);
  const int bundleId = fixedBundleDao_->insertBundle(name, note);
  fixedBundleDao_->clearItems(bundleId);
  for (int modId : modIds) {
    fixedBundleDao_->addItem(bundleId, modId);
  }
  tx.commit();
  return bundleId;
}

void RepositoryService::updateFixedBundle(int bundleId, const std::string& name, const std::vector<int>& modIds,
                                          const std::optional<std::string>& note) {
  Db::Tx tx(*db_);
  fixedBundleDao_->updateBundle(bundleId, name, note);
  fixedBundleDao_->clearItems(bundleId);
  for (int modId : modIds) {
    fixedBundleDao_->addItem(bundleId, modId);
  }
  tx.commit();
}

void RepositoryService::deleteFixedBundle(int bundleId) {
  fixedBundleDao_->deleteBundle(bundleId);
}

std::vector<SavedSchemeRow> RepositoryService::listSavedSchemes() const {
  return savedSchemeDao_->listAll();
}

std::vector<SavedSchemeItemRow> RepositoryService::listSavedSchemeItems(int schemeId) const {
  return savedSchemeDao_->listItems(schemeId);
}

int RepositoryService::createSavedScheme(const std::string& name, double budgetMb,
                                         const std::vector<SavedSchemeItemRow>& items) {
  Db::Tx tx(*db_);
  const int schemeId = savedSchemeDao_->insert(name, budgetMb);
  for (const auto& item : items) {
    SavedSchemeItemRow row = item;
    row.scheme_id = schemeId;
    savedSchemeDao_->addItem(row);
  }
  tx.commit();
  return schemeId;
}

void RepositoryService::updateSavedSchemeItems(int schemeId, const std::vector<SavedSchemeItemRow>& items) {
  Db::Tx tx(*db_);
  savedSchemeDao_->clearItems(schemeId);
  for (const auto& item : items) {
    SavedSchemeItemRow row = item;
    row.scheme_id = schemeId;
    savedSchemeDao_->addItem(row);
  }
  tx.commit();
}

void RepositoryService::deleteSavedScheme(int schemeId) {
  savedSchemeDao_->deleteScheme(schemeId);
}
