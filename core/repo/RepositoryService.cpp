#include "core/repo/RepositoryService.h"
#include <algorithm>
#include <cctype>
#include <utility>
#include <unordered_set>

namespace {

// 归一化 TAG 名称：去空白、去重复
std::vector<std::string> normalizeTagNames(const std::vector<std::string>& tagNames) {
  std::vector<std::string> filtered;
  filtered.reserve(tagNames.size());
  std::unordered_set<std::string> seen;
  for (const auto& raw : tagNames) {
    std::string trimmed = raw;
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char c) { return !std::isspace(c); }));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), trimmed.end());
    if (trimmed.empty()) continue;
    if (seen.insert(trimmed).second) {
      filtered.push_back(std::move(trimmed));
    }
  }
  return filtered;
}

// 确保传入的 TAG 集合都有合法 ID
std::vector<int> ensureTagIds(TagDao& tagDao, const std::vector<std::string>& tagNames) {
  auto normalized = normalizeTagNames(tagNames);
  std::vector<int> ids;
  ids.reserve(normalized.size());
  for (const auto& name : normalized) {
    ids.push_back(tagDao.ensureTagId(name));
  }
  return ids;
}

// 以替换方式重建 MOD 的 TAG 绑定
void replaceTagsForMod(TagDao& tagDao, int modId, const std::vector<int>& tagIds) {
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
  // 直接透传 DAO 查询
  return repoDao_->listVisible();
}

int RepositoryService::createModWithTags(const ModRow& mod, const std::vector<std::string>& tagNames) {
  // 新建 MOD 并绑定 TAG，全程事务保证一致性
  Db::Tx tx(*db_);
  const int modId = repoDao_->insertMod(mod);
  const auto tagIds = ensureTagIds(*tagDao_, tagNames);
  replaceTagsForMod(*tagDao_, modId, tagIds);
  tx.commit();
  return modId;
}

void RepositoryService::updateModTags(int modId, const std::vector<std::string>& tagNames) {
  // 重新绑定 MOD TAG，先清空再写入
  Db::Tx tx(*db_);
  const auto tagIds = ensureTagIds(*tagDao_, tagNames);
  replaceTagsForMod(*tagDao_, modId, tagIds);
  tx.commit();
}

std::vector<CategoryRow> RepositoryService::listCategories() const {
  // 分类通常直接展示树状结构
  return categoryDao_->listAll();
}

int RepositoryService::createCategory(const std::string& name, std::optional<int> parentId) {
  // 创建分类时不含事务，交由调用方控制上下文
  return categoryDao_->insert(name, parentId);
}

void RepositoryService::updateCategory(int id, const std::string& name, std::optional<int> parentId) {
  // 更新分类属性
  categoryDao_->update(id, name, parentId);
}

std::vector<TagRow> RepositoryService::listTags() const {
  // 供 TAG 列表管理页面使用
  return tagDao_->listAll();
}

std::vector<TagRow> RepositoryService::listTagsForMod(int modId) const {
  // 查询 MOD 的 TAG 明细
  return tagDao_->listByMod(modId);
}

std::vector<ModRelationRow> RepositoryService::listRelationsForMod(int modId) const {
  // 聚合关系表数据，便于 UI 呈现
  return relationDao_->listByMod(modId);
}

int RepositoryService::addRelation(const ModRelationRow& relation) {
  // 写入关系前做简单自检，防止自环
  if (relation.a_mod_id == relation.b_mod_id) {
    throw DbError("relation endpoints cannot be the same mod");
  }
  return relationDao_->insert(relation);
}

void RepositoryService::removeRelation(int relationId) {
  // 按主键删除关系
  relationDao_->removeById(relationId);
}

void RepositoryService::removeRelation(int aModId, int bModId, const std::string& type) {
  // 删除指定关系对
  relationDao_->removeBetween(aModId, bModId, type);
}

std::vector<SelectionRow> RepositoryService::listSelections() const {
  // 获取组合方案列表
  return selectionDao_->listAll();
}

std::vector<SelectionItemRow> RepositoryService::listSelectionItems(int selectionId) const {
  // 查询组合条目，用于预览或编辑
  return selectionDao_->listItems(selectionId);
}

int RepositoryService::createSelection(const std::string& name, double budgetMb, const std::vector<SelectionItemRow>& items) {
  // 创建组合和条目，事务保证组合和条目写入同步成功
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
  // 通过清空再写入的方式覆盖组合条目
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
  // 删除组合时，由数据库负责清理关联条目
  selectionDao_->deleteSelection(selectionId);
}
