#pragma once
#include "core/db/Db.h"
#include "core/db/Stmt.h"
#include <string>
#include <stdexcept>

/**
 * @file Migrations.h
 * @brief Database schema migrations and seed-data initialization (idempotent).
 */

namespace migrations {

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
      published_at TEXT,
      source TEXT,
      is_deleted INTEGER NOT NULL DEFAULT 0,
      cover_path TEXT,
      file_path TEXT,
      file_hash TEXT,
      size_mb REAL NOT NULL DEFAULT 0.0,
      created_at TEXT NOT NULL DEFAULT (datetime('now')),
      updated_at TEXT NOT NULL DEFAULT (datetime('now')),
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

    CREATE TABLE IF NOT EXISTS selections (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      budget_mb REAL NOT NULL DEFAULT 2048.0,
      created_at TEXT NOT NULL DEFAULT (datetime('now'))
    );

    CREATE TABLE IF NOT EXISTS selection_items (
      selection_id INTEGER NOT NULL REFERENCES selections(id) ON DELETE CASCADE,
      mod_id INTEGER NOT NULL REFERENCES mods(id) ON DELETE CASCADE,
      is_locked INTEGER NOT NULL DEFAULT 0,
      PRIMARY KEY(selection_id, mod_id)
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
  db.exec(R"SQL(
    INSERT OR IGNORE INTO categories(parent_id, name) VALUES
      (NULL, 'General'),
      (NULL, 'Characters'),
      (NULL, 'Weapons'),
      (NULL, 'Survivors'),
      (NULL, 'Audio');

    INSERT OR IGNORE INTO tag_groups(name, sort_order) VALUES
      ('二次元', 10),
      ('三次元', 20),
      ('健全度', 30),
      ('获取方式', 40);

    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, 'VRC' FROM tag_groups WHERE name = '二次元';
    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, '明日方舟' FROM tag_groups WHERE name = '二次元';
    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, '崩坏' FROM tag_groups WHERE name = '二次元';
    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, 'BA' FROM tag_groups WHERE name = '二次元';
    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, '碧蓝航线' FROM tag_groups WHERE name = '二次元';
    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, '虚拟主播' FROM tag_groups WHERE name = '二次元';

    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, '军事风' FROM tag_groups WHERE name = '三次元';

    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, '健全' FROM tag_groups WHERE name = '健全度';
    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, '非健全' FROM tag_groups WHERE name = '健全度';

    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, '免费' FROM tag_groups WHERE name = '获取方式';
    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, '付费' FROM tag_groups WHERE name = '获取方式';
    INSERT OR IGNORE INTO tags(group_id, name)
      SELECT id, '定制' FROM tag_groups WHERE name = '获取方式';

    INSERT OR IGNORE INTO selections(id, name, budget_mb) VALUES
      (1, 'Default Selection', 2048.0);

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

