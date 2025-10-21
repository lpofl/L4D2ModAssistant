#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file RepositoryDao.h
 * @brief Data access helpers for the mods table and related views.
 */

/**
 * @brief POD representing a row in the mods table.
 * @note Optional database fields use sentinel values: empty string / zero.
 */
struct ModRow {
  int id{0};
  std::string name;
  std::string author;
  int rating{0};          ///< 0 means unset
  int category_id{0};     ///< 0 means unset
  std::string note;
  std::string published_at;
  std::string source_platform;
  std::string source_url;
  bool is_deleted{false};
  std::string cover_path;
  std::string file_path;
  std::string file_hash;
  double size_mb{0.0};
  std::string created_at;
  std::string updated_at;
};

class RepositoryDao {
public:
  explicit RepositoryDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /// Insert a MOD row (id is ignored and auto generated).
  int insertMod(const ModRow& row);
  /// Update mutable MOD fields (requires valid id).
  void updateMod(const ModRow& row);
  /// Toggle logical deletion flag.
  void setDeleted(int id, bool deleted);
  /// Fetch row by primary key.
  std::optional<ModRow> findById(int id) const;
  /// List all visible (not deleted) mods.
  std::vector<ModRow> listVisible() const;

private:
  std::shared_ptr<Db> db_;
};
