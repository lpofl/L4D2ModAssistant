#pragma once
#include "core/db/Db.h"
#include "core/db/Stmt.h"
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

/**
 * @file Migrations.h
 * @brief Database schema migrations and seed-data initialization (idempotent).
 */

namespace migrations {

using json = nlohmann::json;

namespace detail {

inline std::optional<std::filesystem::path> locateSeedFile(const std::string& filename) {
  std::vector<std::filesystem::path> candidates;
  auto cursor = std::filesystem::current_path();
  for (int depth = 0; depth < 3 && !cursor.empty(); ++depth) {
    candidates.emplace_back(cursor / "setting_config" / filename);
    candidates.emplace_back(cursor / filename);
    cursor = cursor.parent_path();
  }

  for (const auto& candidate : candidates) {
    if (std::filesystem::exists(candidate) && std::filesystem::is_regular_file(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

inline std::optional<json> loadSeedJson(const std::string& filename, std::filesystem::path* resolvedPath = nullptr) {
  auto resolved = locateSeedFile(filename);
  if (!resolved) {
    return std::nullopt;
  }
  if (resolvedPath) {
    *resolvedPath = *resolved;
  }

  std::ifstream in(*resolved);
  if (!in.is_open()) {
    throw DbError("failed to open seed file: " + resolved->string());
  }

  try {
    json parsed;
    in >> parsed;
    return parsed;
  } catch (const std::exception& ex) {
    throw DbError("failed to parse seed file '" + resolved->string() + "': " + std::string(ex.what()));
  }
}

inline int ensureCategory(Db& db, const std::string& name, std::optional<int> parentId) {
  Stmt insert(db, "INSERT OR IGNORE INTO categories(parent_id, name) VALUES (?, ?);");
  if (parentId.has_value()) {
    insert.bind(1, parentId.value());
  } else {
    insert.bindNull(1);
  }
  insert.bind(2, name);
  insert.step();

  Stmt query(db, "SELECT id FROM categories WHERE name = ? AND parent_id IS ?;");
  query.bind(1, name);
  if (parentId.has_value()) {
    query.bind(2, parentId.value());
  } else {
    query.bindNull(2);
  }
  if (query.step()) {
    return query.getInt(0);
  }
  throw DbError("failed to resolve category id for " + name);
}

inline void seedCategoryChildren(Db& db, int parentId, const json& node);

inline void seedSingleCategory(Db& db, const std::string& name, std::optional<int> parentId, const json* children = nullptr) {
  const int catId = ensureCategory(db, name, parentId);
  if (children) {
    seedCategoryChildren(db, catId, *children);
  }
}

inline void seedCategoryChildren(Db& db, int parentId, const json& node) {
  if (node.is_array()) {
    for (const auto& child : node) {
      if (child.is_string()) {
        seedSingleCategory(db, child.get<std::string>(), parentId, nullptr);
      } else if (child.is_object()) {
        auto nameIt = child.find("name");
        if (nameIt != child.end() && nameIt->is_string()) {
          const std::string childName = nameIt->get<std::string>();
          const json* grandchildren = nullptr;
          auto childrenIt = child.find("children");
          if (childrenIt != child.end()) {
            grandchildren = &(*childrenIt);
          }
          seedSingleCategory(db, childName, parentId, grandchildren);
        }
      }
    }
  } else if (node.is_string()) {
    seedSingleCategory(db, node.get<std::string>(), parentId, nullptr);
  }
}

inline bool seedCategoriesFromConfig(Db& db) {
  std::filesystem::path sourcePath;
  auto data = loadSeedJson("init_categories.json", &sourcePath);
  if (!data) {
    return false;
  }

  const auto categoriesIt = data->find("categories");
  if (categoriesIt == data->end() || !categoriesIt->is_object()) {
    throw DbError("seed file '" + sourcePath.string() + "' must contain an object property 'categories'");
  }

  for (const auto& [parentName, children] : categoriesIt->items()) {
    if (parentName.empty()) {
      continue;
    }
    seedSingleCategory(db, parentName, std::nullopt, &children);
  }

  return true;
}

inline int ensureTagGroup(Db& db, const std::string& name, int sortOrder) {
  Stmt upsert(db, R"SQL(
    INSERT INTO tag_groups(name, sort_order)
    VALUES (?, ?)
    ON CONFLICT(name) DO UPDATE SET sort_order = excluded.sort_order;
  )SQL");
  upsert.bind(1, name);
  upsert.bind(2, sortOrder);
  upsert.step();

  Stmt query(db, "SELECT id FROM tag_groups WHERE name = ?;");
  query.bind(1, name);
  if (query.step()) {
    return query.getInt(0);
  }
  throw DbError("failed to resolve tag_group id for " + name);
}

inline void ensureTag(Db& db, int groupId, const std::string& tagName) {
  Stmt insert(db, "INSERT OR IGNORE INTO tags(group_id, name) VALUES (?, ?);");
  insert.bind(1, groupId);
  insert.bind(2, tagName);
  insert.step();
}

inline bool seedTagsFromConfig(Db& db) {
  std::filesystem::path sourcePath;
  auto data = loadSeedJson("init_tags.json", &sourcePath);
  if (!data) {
    return false;
  }

  const auto groupsIt = data->find("tag_groups");
  if (groupsIt == data->end() || !groupsIt->is_object()) {
    throw DbError("seed file '" + sourcePath.string() + "' must contain an object property 'tag_groups'");
  }

  int sortOrder = 0;
  for (const auto& [groupName, tags] : groupsIt->items()) {
    if (groupName.empty()) {
      continue;
    }
    const int groupId = ensureTagGroup(db, groupName, (sortOrder + 1) * 10);
    ++sortOrder;

    if (tags.is_array()) {
      for (const auto& tag : tags) {
        if (tag.is_string()) {
          ensureTag(db, groupId, tag.get<std::string>());
        } else if (tag.is_object()) {
          auto nameIt = tag.find("name");
          if (nameIt != tag.end() && nameIt->is_string()) {
            ensureTag(db, groupId, nameIt->get<std::string>());
          }
        }
      }
    } else if (tags.is_string()) {
      ensureTag(db, groupId, tags.get<std::string>());
    }
  }

  return true;
}

} // namespace detail

inline void ensureMetaTable(Db& db) {
  db.exec(R"SQL(
    CREATE TABLE IF NOT EXISTS app_meta (
      key TEXT PRIMARY KEY,
      value TEXT NOT NULL
    );
    INSERT OR IGNORE INTO app_meta(key, value) VALUES ('schema_version', '0');
  )SQL");
}

inline int currentSchemaVersion(Db& db) {
  Stmt stmt(db, "SELECT value FROM app_meta WHERE key = 'schema_version';");
  if (stmt.step()) {
    const auto val = stmt.getText(0);
    try {
      return std::stoi(val);
    } catch (...) {
      throw DbError("invalid schema_version value: " + val);
    }
  }
  return 0;
}

inline void updateSchemaVersion(Db& db, int version) {
  Stmt stmt(db, "UPDATE app_meta SET value = ? WHERE key = 'schema_version';");
  stmt.bind(1, std::to_string(version));
  stmt.step();
}

inline void applyMigration1(Db& db) {
  Db::Tx tx(db);
  db.exec(R"SQL(
    CREATE TABLE IF NOT EXISTS categories (
      id INTEGER PRIMARY KEY,
      parent_id INTEGER REFERENCES categories(id) ON DELETE SET NULL,
      name TEXT NOT NULL,
      UNIQUE(parent_id, name)
    );

    CREATE TABLE IF NOT EXISTS mods (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      author TEXT,
      rating INTEGER CHECK(rating BETWEEN 1 AND 5),
      category_id INTEGER REFERENCES categories(id),
      note TEXT,
      last_published_at TEXT,
      last_saved_at TEXT,
      status TEXT NOT NULL DEFAULT '最新',
      source_platform TEXT,
      source_url TEXT,
      is_deleted INTEGER NOT NULL DEFAULT 0,
      cover_path TEXT,
      file_path TEXT,
      file_hash TEXT,
      size_mb REAL NOT NULL DEFAULT 0.0,
      integrity TEXT,
      stability TEXT,
      acquisition_method TEXT,
      UNIQUE(file_hash)
    );

    CREATE TABLE IF NOT EXISTS tag_groups (
      id INTEGER PRIMARY KEY,
      name TEXT UNIQUE NOT NULL,
      sort_order INTEGER NOT NULL DEFAULT 0
    );

    CREATE TABLE IF NOT EXISTS tags (
      id INTEGER PRIMARY KEY,
      group_id INTEGER NOT NULL REFERENCES tag_groups(id) ON DELETE CASCADE,
      name TEXT NOT NULL,
      UNIQUE(group_id, name)
    );
    CREATE INDEX IF NOT EXISTS idx_tags_group ON tags(group_id);

    CREATE TABLE IF NOT EXISTS mod_tags (
      mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
      tag_id INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
      PRIMARY KEY(mod_id, tag_id)
    );

    CREATE TABLE IF NOT EXISTS mod_relations (
      id INTEGER PRIMARY KEY,
      a_mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
      b_mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
      type TEXT NOT NULL,
      slot_key TEXT,
      note TEXT,
      CHECK(a_mod_id <> b_mod_id),
      UNIQUE(a_mod_id, b_mod_id, type)
    );
    CREATE INDEX IF NOT EXISTS idx_mod_rel_a    ON mod_relations(a_mod_id);
    CREATE INDEX IF NOT EXISTS idx_mod_rel_b    ON mod_relations(b_mod_id);
    CREATE INDEX IF NOT EXISTS idx_mod_rel_type ON mod_relations(type);

    CREATE TABLE IF NOT EXISTS saved_schemes (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      budget_mb REAL NOT NULL DEFAULT 2048.0,
      created_at TEXT NOT NULL DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS saved_scheme_items (
      scheme_id INTEGER NOT NULL REFERENCES saved_schemes(id) ON DELETE CASCADE,
      mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
      is_locked INTEGER NOT NULL DEFAULT 0,
      PRIMARY KEY(scheme_id, mod_id)
    );
    
    CREATE TABLE IF NOT EXISTS fixed_bundles (
    
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    note TEXT
    );
    
    CREATE TABLE IF NOT EXISTS fixed_bundle_items (
    bundle_id INTEGER NOT NULL REFERENCES fixed_bundles(id) ON DELETE CASCADE,
    mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
    PRIMARY KEY(bundle_id, mod_id)
    );

    CREATE TABLE IF NOT EXISTS strategies (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL UNIQUE,
      json TEXT NOT NULL,
      updated_at TEXT NOT NULL DEFAULT (datetime('now'))
    );

    CREATE VIEW IF NOT EXISTS v_mods_visible AS
    SELECT * FROM mods WHERE is_deleted = 0;
  )SQL");
  const bool categoriesSeededFromFile = detail::seedCategoriesFromConfig(db);
  if (!categoriesSeededFromFile) {
    db.exec(R"SQL(
      INSERT OR IGNORE INTO categories(parent_id, name) VALUES
        (NULL, 'General'),
        (NULL, 'Characters'),
        (NULL, 'Weapons'),
        (NULL, 'Survivors'),
        (NULL, 'Audio');
    )SQL");
  }

  const bool tagsSeededFromFile = detail::seedTagsFromConfig(db);
  if (!tagsSeededFromFile) {
    db.exec(R"SQL(
      INSERT OR IGNORE INTO tag_groups(name, sort_order) VALUES
        ('Anime', 10),
        ('Realistic', 20),
        ('Maturity', 30);
    )SQL");
    db.exec(R"SQL(
      INSERT OR IGNORE INTO tags(group_id, name)
        SELECT id, 'VRC' FROM tag_groups WHERE name = 'Anime';
      INSERT OR IGNORE INTO tags(group_id, name)
        SELECT id, 'Arknights' FROM tag_groups WHERE name = 'Anime';
      INSERT OR IGNORE INTO tags(group_id, name)
        SELECT id, 'Honkai' FROM tag_groups WHERE name = 'Anime';
      INSERT OR IGNORE INTO tags(group_id, name)
        SELECT id, 'BA' FROM tag_groups WHERE name = 'Anime';
      INSERT OR IGNORE INTO tags(group_id, name)
        SELECT id, 'Azur Lane' FROM tag_groups WHERE name = 'Anime';
      INSERT OR IGNORE INTO tags(group_id, name)
        SELECT id, 'VTuber' FROM tag_groups WHERE name = 'Anime';

      INSERT OR IGNORE INTO tags(group_id, name)
        SELECT id, 'Military' FROM tag_groups WHERE name = 'Realistic';

      INSERT OR IGNORE INTO tags(group_id, name)
        SELECT id, 'Safe' FROM tag_groups WHERE name = 'Maturity';
      INSERT OR IGNORE INTO tags(group_id, name)
        SELECT id, 'NSFW' FROM tag_groups WHERE name = 'Maturity';
    )SQL");
  }

  db.exec(R"SQL(
    INSERT OR IGNORE INTO saved_schemes(id, name, budget_mb) VALUES
      (1, 'Default Scheme', 2048.0);
  )SQL");

  db.exec(R"SQL(
    INSERT OR IGNORE INTO strategies(name, json) VALUES
      ('Default', '{"name":"Default","rules":[]}');
  )SQL");
  updateSchemaVersion(db, 1);
  tx.commit();
}

} // namespace migrations

/**
 * @brief Run built-in migrations to create or upgrade schema objects.
 * @param db Open database connection.
 */
inline void runMigrations(Db& db) {
  migrations::ensureMetaTable(db);
  auto current = migrations::currentSchemaVersion(db);
  if (current < 1) {
    migrations::applyMigration1(db);
  }
}
