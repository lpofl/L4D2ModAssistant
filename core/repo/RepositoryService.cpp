#include "core/repo/RepositoryService.h"
#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_set>

namespace {

// 去除首尾空白，返回新的 std::string
std::string trimCopy(std::string_view text) {
  auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
  auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) { return std::isspace(c); }).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

// 归一化 TAG 描述：去重、去空白
std::vector<TagDescriptor> normalizeDescriptors(const std::vector<TagDescriptor>& tags) {
  std::vector<TagDescriptor> normalized;
  normalized.reserve(tags.size());
  std::unordered_set<std::string> dedup;

  for (const auto& tag : tags) {
    std::string group = trimCopy(tag.group);
    std::string value = trimCopy(tag.tag);
    if (group.empty() || value.empty()) {
      continue;
    }
    std::string key = group + '\0' + value;
    if (dedup.insert(key).second) {
      normalized.push_back({std::move(group), std::move(value)});
    }
  }
  return normalized;
}

// 确保 TAG 组与条目存在，返回对应 ID 列表
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

// 使用新的 TAG 集合替换 MOD 的现有绑定关系
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
    selectionDao_(std::make_unique<SelectionDao>(db_)) {}

std::vector<ModRow> RepositoryService::listVisible() const {
  return repoDao_->listVisible();
}

int RepositoryService::createModWithTags(const ModRow& mod, const std::vector<TagDescriptor>& tags) {
  Db::Tx tx(*db_);
  const int modId = repoDao_->insertMod(mod);
  const auto tagIds = ensureTagIds(*tagDao_, tags);
  replaceModTags(*tagDao_, modId, tagIds);
  tx.commit();
  return modId;
}

void RepositoryService::updateModTags(int modId, const std::vector<TagDescriptor>& tags) {
  Db::Tx tx(*db_);
  const auto tagIds = ensureTagIds(*tagDao_, tags);
  replaceModTags(*tagDao_, modId, tagIds);
  tx.commit();
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

std::vector<SelectionRow> RepositoryService::listSelections() const {
  return selectionDao_->listAll();
}

std::vector<SelectionItemRow> RepositoryService::listSelectionItems(int selectionId) const {
  return selectionDao_->listItems(selectionId);
}

int RepositoryService::createSelection(const std::string& name, double budgetMb, const std::vector<SelectionItemRow>& items) {
  Db::Tx tx(*db_);
  const int selectionId = selectionDao_->insert(name, budgetMb);
  for (const auto& item : items) {
    SelectionItemRow row = item;
    row.selection_id = selectionId;
    selectionDao_->addItem(row);
  }
  tx.commit();
  return selectionId;
}

void RepositoryService::updateSelectionItems(int selectionId, const std::vector<SelectionItemRow>& items) {
  Db::Tx tx(*db_);
  selectionDao_->clearItems(selectionId);
  for (const auto& item : items) {
    SelectionItemRow row = item;
    row.selection_id = selectionId;
    selectionDao_->addItem(row);
  }
  tx.commit();
}

void RepositoryService::deleteSelection(int selectionId) {
  selectionDao_->deleteSelection(selectionId);
}

