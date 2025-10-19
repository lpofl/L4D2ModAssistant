#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file FixedBundleDao.h
 * @brief DAO for fixed_bundles and fixed_bundle_items tables.
 */

/// 固定搭配本体，仅包含名称与备注
struct FixedBundleRow {
  int id;
  std::string name;
  std::optional<std::string> note;
};

/// 固定搭配内的 MOD 绑定
struct FixedBundleItemRow {
  int bundle_id;
  int mod_id;
};

class FixedBundleDao {
public:
  explicit FixedBundleDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /// 新建固定搭配；note 允许为空
  int insertBundle(const std::string& name, const std::optional<std::string>& note);
  /// 更新固定搭配的名称与备注
  void updateBundle(int id, const std::string& name, const std::optional<std::string>& note);
  /// 删除固定搭配（级联清理条目）
  void deleteBundle(int id);

  /// 返回所有固定搭配
  std::vector<FixedBundleRow> listBundles() const;

  /// 清空固定搭配的 MOD 关联
  void clearItems(int bundleId);
  /// 添加 MOD 到固定搭配
  void addItem(int bundleId, int modId);
  /// 从固定搭配中移除 MOD
  void removeItem(int bundleId, int modId);
  /// 查询固定搭配包含的 MOD
  std::vector<FixedBundleItemRow> listItems(int bundleId) const;

private:
  std::shared_ptr<Db> db_;
};
