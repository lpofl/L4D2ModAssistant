#include "core/repo/RepositoryService.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_set>
#include <utility>

/**
 * @file RepositoryService.cpp
 * @brief 实现了 RepositoryService 类中定义的方法。
 */

namespace {

/**
 * @brief 复制并移除字符串两端的空白字符。
 */
std::string trimCopy(std::string_view text) {
  auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
  auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) { return std::isspace(c); }).base();
  if (begin >= end) return {};
  return std::string(begin, end);
}

/**
 * @brief 标准化处理标签描述符列表，移除空白并去重。
 */
std::vector<TagDescriptor> normalizeDescriptors(const std::vector<TagDescriptor>& tags) {
  std::vector<TagDescriptor> normalized;
  normalized.reserve(tags.size());
  std::unordered_set<std::string> dedup;

  for (const auto& tag : tags) {
    std::string group = trimCopy(tag.group);
    std::string value = trimCopy(tag.tag);
    if (group.empty() || value.empty()) continue;
    // 使用 group + value 作为唯一键进行去重
    std::string key = group + '\0' + value;
    if (dedup.insert(key).second) {
      normalized.push_back({std::move(group), std::move(value)});
    }
  }
  return normalized;
}

/**
 * @brief 确保给定的标签描述符在数据库中存在，如果不存在则创建，并返回它们的ID列表。
 * @param tagDao 标签DAO的引用。
 * @param tags 标签描述符列表。
 * @return 对应的标签ID列表。
 */
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

/**
 * @brief 替换一个MOD的所有标签绑定（先清空后添加）。
 */
void replaceModTags(TagDao& tagDao, int modId, const std::vector<int>& tagIds) {
  tagDao.clearTagsForMod(modId);
  for (int tagId : tagIds) {
    tagDao.addTagToMod(modId, tagId);
  }
}

} // namespace

RepositoryService::RepositoryService(std::shared_ptr<Db> db)
    : db_(std::move(db)),
      // 初始化所有DAO对象
      repoDao_(std::make_unique<RepositoryDao>(db_)),
      categoryDao_(std::make_unique<CategoryDao>(db_)),
      tagDao_(std::make_unique<TagDao>(db_)),
      relationDao_(std::make_unique<ModRelationDao>(db_)),
      savedSchemeDao_(std::make_unique<SavedSchemeDao>(db_)),
      fixedBundleDao_(std::make_unique<FixedBundleDao>(db_)),
      gameModDao_(std::make_unique<GameModDao>(db_)) {}

// --- MOD 管理 ---

std::vector<ModRow> RepositoryService::listVisible() const {
  return repoDao_->listAll(false);
}

std::vector<ModRow> RepositoryService::listAll(bool includeDeleted) const {
  return repoDao_->listAll(includeDeleted);
}

std::optional<ModRow> RepositoryService::findMod(int modId) const {
  return repoDao_->findById(modId);
}

int RepositoryService::createModWithTags(const ModRow& mod, const std::vector<TagDescriptor>& tags) {
  // 检查文件哈希是否已存在，防止重复
  if (!mod.file_hash.empty()) {
    if (repoDao_->findByFileHash(mod.file_hash)) {
      throw DbError("A mod with the same file hash already exists.");
    }
  }

  // 在事务中创建MOD并绑定标签
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
  // 在事务中更新MOD并刷新标签
  Db::Tx tx(*db_);
  repoDao_->updateMod(mod);
  const auto tagIds = ensureTagIds(*tagDao_, tags);
  replaceModTags(*tagDao_, mod.id, tagIds);
  tx.commit();
}

void RepositoryService::updateModTags(int modId, const std::vector<TagDescriptor>& tags) {
  // 在事务中仅更新标签
  Db::Tx tx(*db_);
  const auto tagIds = ensureTagIds(*tagDao_, tags);
  replaceModTags(*tagDao_, modId, tagIds);
  tx.commit();
}

void RepositoryService::setModDeleted(int modId, bool deleted) {
  repoDao_->setDeleted(modId, deleted);
}

void RepositoryService::clearDeletedMods() {
  repoDao_->deleteDeletedMods();
}

// --- 分类管理 ---

std::vector<CategoryRow> RepositoryService::listCategories() const {
  return categoryDao_->listAll();
}

int RepositoryService::createCategory(const std::string& name, std::optional<int> parentId) {
  return categoryDao_->insert(name, parentId);
}

void RepositoryService::updateCategory(int id, const std::string& name, std::optional<int> parentId,
                                       std::optional<int> priority) {
  categoryDao_->update(id, name, parentId, priority);
}

void RepositoryService::deleteCategory(int id) {
  categoryDao_->remove(id);
}

void RepositoryService::swapCategoryPriority(int firstId, int secondId) {
  categoryDao_->swapPriorities(firstId, secondId);
}

// --- 标签管理 ---

std::vector<TagGroupRow> RepositoryService::listTagGroups() const {
  return tagDao_->listGroups();
}

int RepositoryService::createTagGroup(const std::string& name) {
  // 计算新标签组的下一个可用优先级
  const auto groups = tagDao_->listGroups();
  int nextPriority = 10;
  for (const auto& group : groups) {
    nextPriority = std::max(nextPriority, group.priority + 10);
  }
  return tagDao_->insertGroup(name, nextPriority);
}

void RepositoryService::renameTagGroup(int groupId, const std::string& name) {
  tagDao_->updateGroup(groupId, name);
}

bool RepositoryService::deleteTagGroup(int groupId) {
  return tagDao_->removeGroup(groupId);
}

std::vector<TagWithGroupRow> RepositoryService::listTags() const {
  return tagDao_->listAllWithGroup();
}

std::vector<TagRow> RepositoryService::listTagsInGroup(int groupId) const {
  return tagDao_->listByGroup(groupId);
}

int RepositoryService::createTag(int groupId, const std::string& name) {
  return tagDao_->insertTag(groupId, name);
}

void RepositoryService::renameTag(int tagId, const std::string& name) {
  tagDao_->updateTag(tagId, name);
}

bool RepositoryService::deleteTag(int tagId) {
  return tagDao_->removeTag(tagId);
}

std::vector<TagWithGroupRow> RepositoryService::listTagsForMod(int modId) const {
  return tagDao_->listByMod(modId);
}

// --- MOD关系管理 ---

std::vector<ModRelationRow> RepositoryService::listRelationsForMod(int modId) const {
  return relationDao_->listByMod(modId);
}

int RepositoryService::addRelation(const ModRelationRow& relation) {
  // 一个MOD不能与自身建立关系
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

void RepositoryService::replaceRelationsForMod(int modId, const std::vector<ModRelationRow>& relations) {
  // 以事务方式刷新指定 MOD 的全部关系，防止中途失败留下不一致数据
  Db::Tx tx(*db_);
  // 先删除该 MOD 的所有旧关系
  auto existing = relationDao_->listByMod(modId);
  for (const auto& rel : existing) {
    relationDao_->removeById(rel.id);
  }
  // 再添加所有新关系
  for (const auto& rel : relations) {
    addRelation(rel);
  }
  tx.commit();
}

// --- 游戏目录缓存管理 ---

std::vector<GameModRow> RepositoryService::listGameMods() const {
  return gameModDao_->listAll();
}

void RepositoryService::replaceGameModsForSource(const std::string& source, const std::vector<GameModRow>& rows) {
  gameModDao_->replaceForSource(source, rows);
}

void RepositoryService::upsertGameMod(const GameModRow& row) {
  gameModDao_->upsert(row);
}

void RepositoryService::removeGameModsExcept(const std::string& source, const std::vector<std::string>& keepPaths) {
  gameModDao_->removeByPaths(source, keepPaths);
}

// --- 固定搭配管理 ---

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
  fixedBundleDao_->clearItems(bundleId); // 确保是空的
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
  // 使用“先清空后添加”的模式更新条目
  fixedBundleDao_->clearItems(bundleId);
  for (int modId : modIds) {
    fixedBundleDao_->addItem(bundleId, modId);
  }
  tx.commit();
}

void RepositoryService::deleteFixedBundle(int bundleId) {
  fixedBundleDao_->deleteBundle(bundleId);
}

// --- 已存方案管理 ---

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
  // 使用“先清空后添加”的模式更新条目
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