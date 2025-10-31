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

inline int ensureCategory(Db& db,
                          const std::string& name,
                          std::optional<int> parentId,
                          int priority) {
  int resolvedPriority = priority;
  if (resolvedPriority <= 0) {
    Stmt next(db, "SELECT COALESCE(MAX(priority), 0) FROM categories WHERE parent_id IS ?;");
    if (parentId.has_value()) {
      next.bind(1, parentId.value());
    } else {
      next.bindNull(1);
    }
    next.step();
    resolvedPriority = next.getInt(0) + 10;
  }

  Stmt upsert(db, R"SQL(
    INSERT INTO categories(parent_id, name, priority)
    VALUES (?, ?, ?)
    ON CONFLICT(parent_id, name) DO UPDATE SET
      priority = excluded.priority
  )SQL");
  if (parentId.has_value()) {
    upsert.bind(1, parentId.value());
  } else {
    upsert.bindNull(1);
  }
  upsert.bind(2, name);
  upsert.bind(3, resolvedPriority);
  upsert.step();

  Stmt query(db, "SELECT id FROM categories WHERE parent_id IS ? AND name = ?;");
  if (parentId.has_value()) {
    query.bind(1, parentId.value());
  } else {
    query.bindNull(1);
  }
  query.bind(2, name);
  if (query.step()) {
    return query.getInt(0);
  }
  throw DbError("failed to resolve category id for " + name);
}

inline int extractPriority(const json& node, int fallback) {
  auto it = node.find("priority");
  if (it != node.end() && it->is_number_integer()) {
    return it->get<int>();
  }
  return fallback;
}

inline void seedCategoryNode(Db& db, const json& node, std::optional<int> parentId) {
  if (!node.is_object()) {
    throw DbError("category entry must be an object");
  }
  const auto nameIt = node.find("name");
  if (nameIt == node.end() || !nameIt->is_string()) {
    throw DbError("category entry must contain a string 'name'");
  }
  const std::string name = nameIt->get<std::string>();
  const int priority = extractPriority(node, 0);
  const int categoryId = ensureCategory(db, name, parentId, priority);

  const auto itemsIt = node.find("items");
  if (itemsIt != node.end()) {
    if (!itemsIt->is_array()) {
      throw DbError("category 'items' must be an array");
    }
    for (const auto& child : *itemsIt) {
      seedCategoryNode(db, child, categoryId);
    }
  }
}

inline bool seedCategoriesFromConfig(Db& db) {
  std::filesystem::path sourcePath;
  auto data = loadSeedJson("init_categories.json", &sourcePath);
  if (!data) {
    return false;
  }

  const auto categoriesIt = data->find("categories");
  if (categoriesIt == data->end() || !categoriesIt->is_array()) {
    throw DbError("seed file '" + sourcePath.string() + "' must contain an array property 'categories'");
  }

  for (const auto& root : *categoriesIt) {
    seedCategoryNode(db, root, std::nullopt);
  }

  return true;
}

inline int ensureTagGroup(Db& db,
                          const std::string& name,
                          int priority) {
  int resolvedPriority = priority;
  if (resolvedPriority <= 0) {
    Stmt next(db, "SELECT COALESCE(MAX(priority), 0) FROM tag_groups;");
    next.step();
    resolvedPriority = next.getInt(0) + 10;
  }

  Stmt upsert(db, R"SQL(
    INSERT INTO tag_groups(name, priority)
    VALUES (?, ?)
    ON CONFLICT(name) DO UPDATE SET
      priority = excluded.priority
  )SQL");
  upsert.bind(1, name);
  upsert.bind(2, resolvedPriority);
  upsert.step();

  Stmt query(db, "SELECT id FROM tag_groups WHERE name = ?;");
  query.bind(1, name);
  if (query.step()) {
    return query.getInt(0);
  }
  throw DbError("failed to resolve tag_group id for " + name);
}

inline void ensureTag(Db& db,
                      int groupId,
                      const std::string& tagName,
                      int priority) {
  int resolvedPriority = priority;
  if (resolvedPriority <= 0) {
    Stmt next(db, "SELECT COALESCE(MAX(priority), 0) FROM tags WHERE group_id = ?;");
    next.bind(1, groupId);
    next.step();
    resolvedPriority = next.getInt(0) + 10;
  }

  Stmt upsert(db, R"SQL(
    INSERT INTO tags(group_id, name, priority)
    VALUES (?, ?, ?)
    ON CONFLICT(group_id, name) DO UPDATE SET
      priority = excluded.priority
  )SQL");
  upsert.bind(1, groupId);
  upsert.bind(2, tagName);
  upsert.bind(3, resolvedPriority);
  upsert.step();
}

inline int extractTagPriority(const json& node) {
  auto sortIt = node.find("priority");
  if (sortIt != node.end() && sortIt->is_number_integer()) {
    return sortIt->get<int>();
  }
  return 0;
}

inline bool seedTagsFromConfig(Db& db) {
  std::filesystem::path sourcePath;
  auto data = loadSeedJson("init_tags.json", &sourcePath);
  if (!data) {
    return false;
  }

  const auto groupsIt = data->find("tag_groups");
  if (groupsIt == data->end() || !groupsIt->is_array()) {
    throw DbError("seed file '" + sourcePath.string() + "' must contain an array property 'tag_groups'");
  }

  for (const auto& groupNode : *groupsIt) {
    if (!groupNode.is_object()) {
      throw DbError("tag_groups entries must be objects");
    }
    const auto groupNameIt = groupNode.find("name");
    if (groupNameIt == groupNode.end() || !groupNameIt->is_string()) {
      throw DbError("tag group must contain a string 'name'");
    }
    const std::string groupName = groupNameIt->get<std::string>();
    const int groupPriority = extractTagPriority(groupNode);
    const int groupId = ensureTagGroup(db, groupName, groupPriority);

    const auto tagsIt = groupNode.find("tags");
    if (tagsIt == groupNode.end()) {
      continue;
    }
    if (!tagsIt->is_array()) {
      throw DbError("tag group 'tags' must be an array");
    }
    for (const auto& tagNode : *tagsIt) {
      if (!tagNode.is_object()) {
        throw DbError("tag entry must be an object");
      }
      const auto tagNameIt = tagNode.find("name");
      if (tagNameIt == tagNode.end() || !tagNameIt->is_string()) {
        throw DbError("tag entry must contain a string 'name'");
      }
      const std::string tagName = tagNameIt->get<std::string>();
      const int tagPriority = extractTagPriority(tagNode);
      ensureTag(db, groupId, tagName, tagPriority);
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
      priority INTEGER NOT NULL,
      UNIQUE(parent_id, name),
      UNIQUE(parent_id, priority)
    );
    CREATE INDEX IF NOT EXISTS idx_categories_parent_priority ON categories(parent_id, priority, id);

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
      name TEXT NOT NULL UNIQUE,
      priority INTEGER NOT NULL UNIQUE
    );

    CREATE TABLE IF NOT EXISTS tags (
      id INTEGER PRIMARY KEY,
      group_id INTEGER NOT NULL REFERENCES tag_groups(id) ON DELETE CASCADE,
      name TEXT NOT NULL,
      priority INTEGER NOT NULL,
      UNIQUE(group_id, name),
      UNIQUE(group_id, priority)
    );
    CREATE INDEX IF NOT EXISTS idx_tags_group ON tags(group_id);
    CREATE INDEX IF NOT EXISTS idx_tags_group_priority ON tags(group_id, priority, id);

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
      INSERT OR IGNORE INTO categories(parent_id, name, priority) VALUES
        (NULL, 'General', 10),
        (NULL, 'Characters', 20),
        (NULL, 'Weapons', 30),
        (NULL, 'Survivors', 40),
        (NULL, 'Audio', 50);
    )SQL");
  }

  const bool tagsSeededFromFile = detail::seedTagsFromConfig(db);
  if (!tagsSeededFromFile) {
    db.exec(R"SQL(
      INSERT OR IGNORE INTO tag_groups(name, priority) VALUES
        ('Anime', 10),
        ('Realistic', 20),
        ('Maturity', 30);
    )SQL");
    db.exec(R"SQL(
      INSERT OR IGNORE INTO tags(group_id, name, priority)
        SELECT id, 'VRC', 10 FROM tag_groups WHERE name = 'Anime';
      INSERT OR IGNORE INTO tags(group_id, name, priority)
        SELECT id, 'Arknights', 20 FROM tag_groups WHERE name = 'Anime';
      INSERT OR IGNORE INTO tags(group_id, name, priority)
        SELECT id, 'Honkai', 30 FROM tag_groups WHERE name = 'Anime';
      INSERT OR IGNORE INTO tags(group_id, name, priority)
        SELECT id, 'BA', 40 FROM tag_groups WHERE name = 'Anime';
      INSERT OR IGNORE INTO tags(group_id, name, priority)
        SELECT id, 'Azur Lane', 50 FROM tag_groups WHERE name = 'Anime';
      INSERT OR IGNORE INTO tags(group_id, name, priority)
        SELECT id, 'VTuber', 60 FROM tag_groups WHERE name = 'Anime';

      INSERT OR IGNORE INTO tags(group_id, name, priority)
        SELECT id, 'Military', 10 FROM tag_groups WHERE name = 'Realistic';

      INSERT OR IGNORE INTO tags(group_id, name, priority)
        SELECT id, 'Safe', 10 FROM tag_groups WHERE name = 'Maturity';
      INSERT OR IGNORE INTO tags(group_id, name, priority)
        SELECT id, 'NSFW', 20 FROM tag_groups WHERE name = 'Maturity';
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

inline void applyMigration2(Db& db) {
  Db::Tx tx(db);
  db.exec(R"SQL(
    CREATE TABLE IF NOT EXISTS gamemods (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      file_path TEXT NOT NULL UNIQUE,
      source TEXT NOT NULL CHECK(source IN ('addons','workshop')),
      file_size INTEGER NOT NULL DEFAULT 0,
      modified_at TEXT,
      status TEXT NOT NULL DEFAULT '',
      repo_mod_id INTEGER,
      last_scanned_at TEXT NOT NULL DEFAULT (datetime('now')),
      FOREIGN KEY(repo_mod_id) REFERENCES mods(id) ON DELETE SET NULL
    );
    CREATE INDEX IF NOT EXISTS idx_gamemods_source ON gamemods(source);
    CREATE INDEX IF NOT EXISTS idx_gamemods_repo ON gamemods(repo_mod_id);
  )SQL");
  updateSchemaVersion(db, 2);
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
    current = migrations::currentSchemaVersion(db);
  }
  if (current < 2) {
    migrations::applyMigration2(db);
  }
}
