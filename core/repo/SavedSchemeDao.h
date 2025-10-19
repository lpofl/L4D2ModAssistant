#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file SavedSchemeDao.h
 * @brief DAO for saved_schemes and saved_scheme_items tables.
 */

struct SavedSchemeRow {
  int id;
  std::string name;
  double budget_mb;
  std::string created_at;
};

struct SavedSchemeItemRow {
  int scheme_id;
  int mod_id;
  bool is_locked;
};

class SavedSchemeDao {
public:
  explicit SavedSchemeDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  int insert(const std::string& name, double budgetMb);
  void updateName(int id, const std::string& name);
  void updateBudget(int id, double budgetMb);
  void deleteScheme(int id);

  std::vector<SavedSchemeRow> listAll() const;
  std::optional<SavedSchemeRow> findById(int id) const;

  void clearItems(int schemeId);
  void addItem(const SavedSchemeItemRow& item);
  void removeItem(int schemeId, int modId);
  std::vector<SavedSchemeItemRow> listItems(int schemeId) const;

private:
  std::shared_ptr<Db> db_;
};

