#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file CategoryDao.h
 * @brief 封装了对分类（Category）数据表的增删查改（CRUD）操作。
 * @note “分类”以层级结构（树状）组织，用于管理MOD。
 */

/**
 * @brief 代表 categories 数据表中的一行记录。
 */
struct CategoryRow {
  int id = 0;  ///< 分类唯一ID（主键）
  std::optional<int> parent_id; ///< 父分类ID，若为顶级分类则为空
  std::string name; ///< 分类名称
  int priority = 0; ///< 同级分类中的排序优先级
};

/**
 * @brief 分类数据访问对象（DAO）。
 * @details 提供了对 categories 数据表进行操作的各种方法。
 */
class CategoryDao {
public:
  /**
   * @brief 构造一个新的 CategoryDao 对象。
   * @param db 数据库连接的共享指针。
   */
  explicit CategoryDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /**
   * @brief 插入一个新的分类。
   * @param name 新分类的名称。
   * @param parentId 可选的父分类ID。
   * @return 新插入分类的自增主键ID。
   */
  int insert(const std::string& name, std::optional<int> parentId);

  /**
   * @brief 更新一个已存在分类的信息。
   * @param id 要更新的分类ID。
   * @param name 新的分类名称。
   * @param parentId 新的父分类ID。
   * @param priority 可选的，新的同级排序优先级。
   */
  void update(int id, const std::string& name, std::optional<int> parentId, std::optional<int> priority);

  /**
   * @brief 查询并返回所有的分类记录。
   * @return 按父节点ID和优先级排序的全部分类列表。
   */
  std::vector<CategoryRow> listAll() const;

  /**
   * @brief 根据ID查找一个分类。
   * @param id 要查找的分类ID。
   * @return 如果找到，则返回包含分类信息的 CategoryRow，否则返回 std::nullopt。
   */
  std::optional<CategoryRow> findById(int id) const;

  /**
   * @brief 删除一个分类及其所有子分类。
   * @details 此操作会递归删除所有子分类，并清除游戏模组（GameMod）对这些分类的引用。
   * @param id 要删除的顶级分类ID。
   */
  void remove(int id);

  /**
   * @brief 交换两个同级分类的优先级。
   * @param firstId 第一个分类的ID。
   * @param secondId 第二个分类的ID。
   */
  void swapPriorities(int firstId, int secondId);

private:
  std::shared_ptr<Db> db_; ///< 数据库连接实例
};