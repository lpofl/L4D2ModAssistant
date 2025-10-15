

#include "core/repo/RepositoryDao.h"

/**
 * @file RepositoryDao.cpp
 * @brief RepositoryDao 的实现，包含基础的插入与查询。
 */

int RepositoryDao::insertMod(const ModRow& m) {
  // 使用事务保证插入的原子性
  Db::Tx tx(*db_);

  // 预编译插入语句并绑定参数
  Stmt s(*db_, "INSERT INTO mods(name, rating, category_id, size_mb, is_deleted, file_path, file_hash) VALUES(?,?,?,?,?,?,?);");
  s.bind(1, m.name); s.bind(2, m.rating); s.bind(3, m.category_id);
  s.bind(4, m.size_mb); s.bind(5, m.is_deleted ? 1 : 0); s.bind(6, m.file_path); s.bind(7, m.file_hash);
  s.step();

  // 获取自增主键 ID
  int id = (int)sqlite3_last_insert_rowid(db_->raw());
  tx.commit();
  return id;
}

std::vector<ModRow> RepositoryDao::listVisible() {
  // 从视图 v_mods_visible 读取未删除条目，COALESCE 处理空值
  Stmt s(*db_, "SELECT id,name,COALESCE(rating,0),COALESCE(category_id,0),size_mb,is_deleted,COALESCE(file_path,''),COALESCE(file_hash,'') FROM v_mods_visible ORDER BY name;");
  std::vector<ModRow> out;
  while (s.step()) {
    out.push_back({
      s.getInt(0), s.getText(1), s.getInt(2), s.getInt(3),
      s.getDouble(4), s.getInt(5)!=0, s.getText(6), s.getText(7)
    });
  }
  return out;
}
