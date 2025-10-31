#include "core/repo/CategoryDao.h"
#include <utility>

/**
 * @file CategoryDao.cpp
 * @brief 实现了 CategoryDao 类中定义的方法。
 */

namespace {

/**
 * @brief 计算新分类的下一个可用优先级。
 * @details 优先级用于决定同级分类的显示顺序。新分类的优先级会比当前同级最高优先级大10。
 * @param db 数据库连接实例。
 * @param parentId 父分类的ID。如果为顶级分类，则为 std::nullopt。
 * @return 计算出的新优先级。
 */
int nextPriority(Db& db, std::optional<int> parentId) {
  if (parentId.has_value()) {
    // 查询指定父分类下的最高优先级
    Stmt stmt(db, "SELECT COALESCE(MAX(priority), 0) FROM categories WHERE parent_id = ?;");
    stmt.bind(1, *parentId);
    stmt.step();
    return stmt.getInt(0) + 10;
  }
  // 查询所有顶级分类的最高优先级
  Stmt stmt(db, "SELECT COALESCE(MAX(priority), 0) FROM categories WHERE parent_id IS NULL;");
  stmt.step();
  return stmt.getInt(0) + 10;
}

} // namespace

int CategoryDao::insert(const std::string& name, std::optional<int> parentId) {
  // 获取新分类的优先级
  const int priority = nextPriority(*db_, parentId);
  Stmt stmt(*db_, "INSERT INTO categories(parent_id, name, priority) VALUES(?, ?, ?);");
  
  // 绑定 parent_id，可能为空
  if (parentId.has_value()) {
    stmt.bind(1, *parentId);
  } else {
    stmt.bindNull(1);
  }
  
  // 绑定分类名称和优先级
  stmt.bind(2, name);
  stmt.bind(3, priority);
  
  stmt.step();
  // 返回新插入行的ID
  return static_cast<int>(sqlite3_last_insert_rowid(db_->raw()));
}

void CategoryDao::update(int id, const std::string& name, std::optional<int> parentId, std::optional<int> priority) {
  // COALESCE(?, priority) 表示如果传入的 priority 为 NULL，则保持原值不变
  Stmt stmt(*db_, "UPDATE categories SET parent_id = ?, name = ?, priority = COALESCE(?, priority) WHERE id = ?;");
  
  if (parentId.has_value()) {
    stmt.bind(1, *parentId);
  } else {
    stmt.bindNull(1);
  }
  
  stmt.bind(2, name);
  
  if (priority.has_value()) {
    stmt.bind(3, *priority);
  } else {
    stmt.bindNull(3);
  }
  
  stmt.bind(4, id);
  stmt.step();
}

std::vector<CategoryRow> CategoryDao::listAll() const {
  // ORDER BY COALESCE(parent_id, 0) 确保顶级分类（parent_id IS NULL）排在前面
  Stmt stmt(*db_, "SELECT id, parent_id, name, priority FROM categories ORDER BY COALESCE(parent_id, 0), priority, id;");
  std::vector<CategoryRow> rows;
  while (stmt.step()) {
    CategoryRow row;
    row.id = stmt.getInt(0);
    row.parent_id = stmt.isNull(1) ? std::optional<int>{} : std::optional<int>{stmt.getInt(1)};
    row.name = stmt.getText(2);
    row.priority = stmt.getInt(3);
    rows.push_back(std::move(row));
  }
  return rows;
}

std::optional<CategoryRow> CategoryDao::findById(int id) const {
  Stmt stmt(*db_, "SELECT id, parent_id, name, priority FROM categories WHERE id = ?;");
  stmt.bind(1, id);
  
  // 如果没有查询到结果，则返回空
  if (!stmt.step()) {
    return std::nullopt;
  }
  
  CategoryRow row;
  row.id = stmt.getInt(0);
  row.parent_id = stmt.isNull(1) ? std::optional<int>{} : std::optional<int>{stmt.getInt(1)};
  row.name = stmt.getText(2);
  row.priority = stmt.getInt(3);
  return row;
}

void CategoryDao::remove(int id) {
  // 开启数据库事务，确保操作的原子性
  Db::Tx tx(*db_);

  // 步骤1: 广度优先遍历，收集目标分类及其所有子孙分类的ID
  std::vector<int> pending{ id };
  std::vector<int> orderedIds; // 存储遍历结果，父节点在前
  while (!pending.empty()) {
    const int current = pending.back();
    pending.pop_back();
    orderedIds.push_back(current);

    // 查找当前分类的所有直接子分类
    Stmt children(*db_, "SELECT id FROM categories WHERE parent_id = ?;");
    children.bind(1, current);
    while (children.step()) {
      pending.push_back(children.getInt(0));
    }
  }

  // 步骤2: 清除 mods 表中对这些待删除分类的引用
  for (int catId : orderedIds) {
    Stmt clearModRef(*db_, "UPDATE mods SET category_id = NULL WHERE category_id = ?;");
    clearModRef.bind(1, catId);
    clearModRef.step();
  }

  // 步骤3: 从叶子节点开始，反向删除分类自身
  for (auto it = orderedIds.rbegin(); it != orderedIds.rend(); ++it) {
    Stmt del(*db_, "DELETE FROM categories WHERE id = ?;");
    del.bind(1, *it);
    del.step();
  }

  // 提交事务
  tx.commit();
}

void CategoryDao::swapPriorities(int firstId, int secondId) {
  if (firstId == secondId) {
    return;
  }

  // 开启数据库事务
  Db::Tx tx(*db_);

  // 定义一个 lambda 函数用于获取指定ID分类的父ID和优先级
  auto fetch = [&](int id) {
    Stmt query(*db_, "SELECT parent_id, priority FROM categories WHERE id = ?;");
    query.bind(1, id);
    if (!query.step()) {
      throw DbError("category not found for swap");
    }
    std::optional<int> parent;
    if (!query.isNull(0)) {
      parent = query.getInt(0);
    }
    const int priority = query.getInt(1);
    return std::make_pair(parent, priority);
  };

  // 获取两个分类的信息
  const auto [parentA, priorityA] = fetch(firstId);
  const auto [parentB, priorityB] = fetch(secondId);
  
  // 确保两个分类在同一层级下，否则无法交换
  if (parentA.has_value() != parentB.has_value() ||
      (parentA.has_value() && parentA.value() != parentB.value())) {
    throw DbError("cannot swap categories from different levels");
  }

  // 步骤1: 将第一个分类的优先级设置为一个临时的、不会冲突的值
  // 这是为了避免在交换过程中违反 UNIQUE 约束
  Stmt temp(*db_, "UPDATE categories SET priority = ? WHERE id = ?;");
  temp.bind(1, -priorityA - 1); // 使用负值作为临时值
  temp.bind(2, firstId);
  temp.step();

  // 步骤2: 将第二个分类的优先级更新为第一个分类的原始优先级
  Stmt updateSecond(*db_, "UPDATE categories SET priority = ? WHERE id = ?;");
  updateSecond.bind(1, priorityA);
  updateSecond.bind(2, secondId);
  updateSecond.step();

  // 步骤3: 将第一个分类的优先级更新为第二个分类的原始优先级
  Stmt restoreFirst(*db_, "UPDATE categories SET priority = ? WHERE id = ?;");
  restoreFirst.bind(1, priorityB);
  restoreFirst.bind(2, firstId);
  restoreFirst.step();

  // 提交事务
  tx.commit();
}