#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file CategoryDao.h
 * @brief Data access helpers for the categories hierarchy.
 * @note 分类表 DAO，用于封装分类树的增删查改操作。
 */

struct CategoryRow {
  int id;
  std::optional<int> parent_id;
  std::string name;
};

class CategoryDao {
public:
  explicit CategoryDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /// 新增分类，可指定父节点；返回自增主键
  int insert(const std::string& name, std::optional<int> parentId);
  /// 更新分类名称与父子关系
  void update(int id, const std::string& name, std::optional<int> parentId);
  /// 按父节点排序返回全量分类
  std::vector<CategoryRow> listAll() const;// 常量成员函数，不修改成员变量
  /// 按主键查询单条分类记录
  std::optional<CategoryRow> findById(int id) const;

private:
  std::shared_ptr<Db> db_;
};
