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

struct FixedBundleRow {
  int id;
  std::string name;
  std::optional<std::string> note;
};

struct FixedBundleItemRow {
  int bundle_id;
  int mod_id;
};

class FixedBundleDao {
public:
  explicit FixedBundleDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  int insertBundle(const std::string& name, const std::optional<std::string>& note);
  void updateBundle(int id, const std::string& name, const std::optional<std::string>& note);
  void deleteBundle(int id);

  std::vector<FixedBundleRow> listBundles() const;

  void clearItems(int bundleId);
  void addItem(int bundleId, int modId);
  void removeItem(int bundleId, int modId);
  std::vector<FixedBundleItemRow> listItems(int bundleId) const;

private:
  std::shared_ptr<Db> db_;
};

