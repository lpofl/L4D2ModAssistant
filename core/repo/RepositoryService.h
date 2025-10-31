#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/repo/CategoryDao.h"
#include "core/repo/FixedBundleDao.h"
#include "core/repo/GameModDao.h"
#include "core/repo/ModRelationDao.h"
#include "core/repo/RepositoryDao.h"
#include "core/repo/SavedSchemeDao.h"
#include "core/repo/TagDao.h"

/**
 * @file RepositoryService.h
 * @brief 将所有DAO聚合为面向业务场景的高级操作。
 * @details 这是数据仓库层的核心服务，为上层业务逻辑（如UI的Presenter）提供统一的数据访问接口。
 *          它封装了所有与数据库表直接交互的DAO对象，并组合它们以完成复杂的业务事务。
 */

/**
 * @brief 用于UI层编辑标签的轻量级描述符。
 */
struct TagDescriptor {
  std::string group; ///< 标签所属的组名
  std::string tag;   ///< 标签名
};

/**
 * @brief 仓库服务类，封装所有数据访问和业务逻辑。
 */
class RepositoryService {
public:
  /**
   * @brief 构造一个新的 RepositoryService 对象。
   * @param db 数据库连接的共享指针。
   */
  explicit RepositoryService(std::shared_ptr<Db> db);

  // --- MOD 管理 ---

  /**
   * @brief 查询所有可见（未被逻辑删除）的MOD。
   * @return 可见MOD的列表。
   */
  std::vector<ModRow> listVisible() const;
  
  /**
   * @brief 查询所有MOD。
   * @param includeDeleted 是否包含已被逻辑删除的MOD。
   * @return MOD列表。
   */
  std::vector<ModRow> listAll(bool includeDeleted = false) const;
  
  /**
   * @brief 根据ID查找MOD。
   * @param modId MOD ID。
   * @return 如果找到则返回MOD信息，否则返回 std::nullopt。
   */
  std::optional<ModRow> findMod(int modId) const;
  
  /**
   * @brief 以原子方式创建一个新的MOD并绑定其标签。
   * @param mod 要创建的MOD的数据。
   * @param tags 要绑定的标签列表。
   * @return 新创建的MOD的ID。
   */
  int createModWithTags(const ModRow& mod, const std::vector<TagDescriptor>& tags);
  
  /**
   * @brief 更新MOD信息并刷新其标签绑定。
   * @param mod 要更新的MOD的数据（必须包含有效ID）。
   * @param tags 新的标签列表，将完全替换旧的标签。
   */
  void updateModWithTags(const ModRow& mod, const std::vector<TagDescriptor>& tags);
  
  /**
   * @brief 仅更新MOD的标签绑定，不修改MOD自身数据。
   * @param modId MOD ID。
   * @param tags 新的标签列表。
   */
  void updateModTags(int modId, const std::vector<TagDescriptor>& tags);
  
  /**
   * @brief 设置MOD的逻辑删除状态。
   * @param modId MOD ID。
   * @param deleted true表示逻辑删除，false表示恢复。
   */
  void setModDeleted(int modId, bool deleted);
  
  /**
   * @brief 永久删除所有已被逻辑删除的MOD。
   */
  void clearDeletedMods();

  // --- 分类管理 ---

  std::vector<CategoryRow> listCategories() const;
  int createCategory(const std::string& name, std::optional<int> parentId);
  void updateCategory(int id, const std::string& name, std::optional<int> parentId,
                      std::optional<int> priority = std::nullopt);
  void deleteCategory(int id);
  void swapCategoryPriority(int firstId, int secondId);

  // --- 标签管理 ---

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

  // --- MOD关系管理 ---

  std::vector<ModRelationRow> listRelationsForMod(int modId) const;
  int addRelation(const ModRelationRow& relation);
  void removeRelation(int relationId);
  void removeRelation(int aModId, int bModId, const std::string& type);
  void replaceRelationsForMod(int modId, const std::vector<ModRelationRow>& relations);

  // --- 游戏目录缓存管理 ---

  std::vector<GameModRow> listGameMods() const;
  void replaceGameModsForSource(const std::string& source, const std::vector<GameModRow>& rows);
  void upsertGameMod(const GameModRow& row);
  void removeGameModsExcept(const std::string& source, const std::vector<std::string>& keepPaths);

  // --- 固定搭配管理 ---

  std::vector<FixedBundleRow> listFixedBundles() const;
  std::vector<FixedBundleItemRow> listFixedBundleItems(int bundleId) const;
  int createFixedBundle(const std::string& name, const std::vector<int>& modIds, const std::optional<std::string>& note);
  void updateFixedBundle(int bundleId, const std::string& name, const std::vector<int>& modIds, const std::optional<std::string>& note);
  void deleteFixedBundle(int bundleId);

  // --- 已存方案管理 ---

  std::vector<SavedSchemeRow> listSavedSchemes() const;
  std::vector<SavedSchemeItemRow> listSavedSchemeItems(int schemeId) const;
  int createSavedScheme(const std::string& name, double budgetMb, const std::vector<SavedSchemeItemRow>& items);
  void updateSavedSchemeItems(int schemeId, const std::vector<SavedSchemeItemRow>& items);
  void deleteSavedScheme(int schemeId);

private:
  std::shared_ptr<Db> db_; ///< 共享的数据库连接实例
  
  // --- Data Access Objects ---
  // 服务通过持有的DAO对象来执行具体的数据库操作
  std::unique_ptr<RepositoryDao> repoDao_;
  std::unique_ptr<CategoryDao> categoryDao_;
  std::unique_ptr<TagDao> tagDao_;
  std::unique_ptr<ModRelationDao> relationDao_;
  std::unique_ptr<SavedSchemeDao> savedSchemeDao_;
  std::unique_ptr<FixedBundleDao> fixedBundleDao_;
  std::unique_ptr<GameModDao> gameModDao_;
};