#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "core/db/Db.h"
#include "core/db/Stmt.h"

/**
 * @file ModRelationDao.h
 * @brief 封装了对“MOD关系”（mod_relations）数据表的操作。
 * @details 此DAO用于维护MOD之间的各种关系，如依赖、冲突等。
 */

/**
 * @brief 代表 mod_relations 数据表中的一行记录。
 */
struct ModRelationRow {
  int id; ///< 关系记录的唯一ID
  int a_mod_id; ///< 关系中的A方MOD ID
  int b_mod_id; ///< 关系中的B方MOD ID
  
  /**
   * @brief 关系类型。
   * - `requires`: A依赖B
   * - `conflicts`: A与B冲突
   * - `homologous`: A与B为同源（互斥）
   * - `custom_master`: A为B的自定义主MOD
   * - `party`: A与B为配套（可多选）
   */
  std::string type;
  
  std::optional<std::string> slot_key; ///< 关联的槽位信息，可以为空
  std::optional<std::string> note; ///< 备注信息，可以为空
};

/**
 * @brief MOD关系数据访问对象（DAO）。
 * @details 提供了对 mod_relations 数据表进行操作的各种方法。
 */
class ModRelationDao {
public:
  /**
   * @brief 构造一个新的 ModRelationDao 对象。
   * @param db 数据库连接的共享指针。
   */
  explicit ModRelationDao(std::shared_ptr<Db> db) : db_(std::move(db)) {}

  /**
   * @brief 插入一条新的MOD关系记录。
   * @param row 包含关系信息的 ModRelationRow 对象。
   * @return 新插入记录的自增主键ID。
   */
  int insert(const ModRelationRow& row);

  /**
   * @brief 根据主键ID删除一条关系记录。
   * @param id 要删除的关系记录ID。
   */
  void removeById(int id);

  /**
   * @brief 删除两个MOD之间指定类型的关系。
   * @details 用于成对解除关系，会同时删除 (A, B) 和 (B, A) 的关系。
   * @param aModId A方MOD ID。
   * @param bModId B方MOD ID。
   * @param type 要删除的关系类型。
   */
  void removeBetween(int aModId, int bModId, const std::string& type);

  /**
   * @brief 查询与指定MOD相关的所有关系。
   * @details 会返回该MOD作为A方或B方的所有关系记录。
   * @param modId 要查询的MOD ID。
   * @return 包含所有相关关系记录的列表。
   */
  std::vector<ModRelationRow> listByMod(int modId) const;

private:
  std::shared_ptr<Db> db_; ///< 数据库连接实例
};