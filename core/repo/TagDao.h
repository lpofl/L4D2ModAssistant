#pragma once
#include <memory>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file TagDao.h
 * @brief DAO for managing tag groups, tags, and mod-tag relations.
 */

struct TagGroupRow {
  int id = 0;
  std::string name;
  int priority = 0;
};

struct TagRow {
  int id = 0;
  int group_id = 0;
  std::string name;
  int priority = 0;
};

struct TagWithGroupRow {
  int id = 0;
  int group_id = 0;
  std::string group_name;
  int group_priority = 0;
  std::string name;
  int priority = 0;
};

class TagDao {
public:
  explicit TagDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  int insertGroup(const std::string& name, int priority);
  void updateGroup(int groupId, const std::string& name);
  bool removeGroup(int groupId);
  int ensureGroupId(const std::string& name);
  std::vector<TagGroupRow> listGroups() const;

  int insertTag(int groupId, const std::string& name);
  void updateTag(int tagId, const std::string& name);
  int ensureTagId(int groupId, const std::string& name);
  std::vector<TagRow> listByGroup(int groupId) const;
  std::vector<TagWithGroupRow> listAllWithGroup() const;
  std::vector<TagWithGroupRow> listByMod(int modId) const;
  void deleteUnused(int tagId);
  bool removeTag(int tagId);

  void clearTagsForMod(int modId);
  void addTagToMod(int modId, int tagId);

private:
  std::shared_ptr<Db> db_;
};
