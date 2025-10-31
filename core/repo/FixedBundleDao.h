#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file FixedBundleDao.h
 * @brief 封装了对“固定搭配”(Fixed Bundle)相关数据表的操作。
 * @details 固定搭配是一组预设的MOD组合。此DAO负责管理 fixed_bundles 和 fixed_bundle_items 两个表。
 */

/**
 * @brief 代表 fixed_bundles 数据表中的一行记录，即一个固定搭配。
 */
struct FixedBundleRow {
  int id; ///< 固定搭配的唯一ID
  std::string name; ///< 固定搭配的名称
  std::optional<std::string> note; ///< 关于该搭配的备注信息，可以为空
};

/**
 * @brief 代表 fixed_bundle_items 数据表中的一行记录，表示搭配与MOD的关联。
 */
struct FixedBundleItemRow {
  int bundle_id; ///< 所属固定搭配的ID
  int mod_id;    ///< 包含的MOD ID
};

/**
 * @brief 固定搭配数据访问对象（DAO）。
 * @details 提供了对 fixed_bundles 和 fixed_bundle_items 数据表进行操作的各种方法。
 */
class FixedBundleDao {
public:
  /**
   * @brief 构造一个新的 FixedBundleDao 对象。
   * @param db 数据库连接的共享指针。
   */
  explicit FixedBundleDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /**
   * @brief 创建一个新的固定搭配。
   * @param name 固定搭配的名称。
   * @param note 可选的备注信息。
   * @return 新创建的固定搭配的ID。
   */
  int insertBundle(const std::string& name, const std::optional<std::string>& note);

  /**
   * @brief 更新一个已存在的固定搭配的名称和备注。
   * @param id 要更新的固定搭配ID。
   * @param name 新的名称。
   * @param note 可选的，新的备注信息。
   */
  void updateBundle(int id, const std::string& name, const std::optional<std::string>& note);

  /**
   * @brief 删除一个固定搭配。
   * @details 此操作会级联删除该搭配下的所有MOD关联条目。
   * @param id 要删除的固定搭配ID。
   */
  void deleteBundle(int id);

  /**
   * @brief 列出所有的固定搭配。
   * @return 包含所有固定搭配信息的列表。
   */
  std::vector<FixedBundleRow> listBundles() const;

  /**
   * @brief 清空一个固定搭配中的所有MOD关联。
   * @param bundleId 要清空的固定搭配ID。
   */
  void clearItems(int bundleId);

  /**
   * @brief 向固定搭配中添加一个MOD。
   * @param bundleId 目标固定搭配的ID。
   * @param modId 要添加的MOD ID。
   */
  void addItem(int bundleId, int modId);

  /**
   * @brief 从固定搭配中移除一个MOD。
   * @param bundleId 目标固定搭配的ID。
   * @param modId 要移除的MOD ID。
   */
  void removeItem(int bundleId, int modId);

  /**
   * @brief 列出一个固定搭配中包含的所有MOD。
   * @param bundleId 要查询的固定搭配ID。
   * @return 包含所有MOD关联信息的列表。
   */
  std::vector<FixedBundleItemRow> listItems(int bundleId) const;

private:
  std::shared_ptr<Db> db_; ///< 数据库连接实例
};