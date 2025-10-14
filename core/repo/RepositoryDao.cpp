
#include "core/repo/RepositoryDao.h"

int RepositoryDao::insertMod(const ModRow& m) {
  Db::Tx tx(*db_);
  Stmt s(*db_, "INSERT INTO mods(name, rating, category_id, size_mb, is_deleted, file_path, file_hash) VALUES(?,?,?,?,?,?,?);");
  s.bind(1, m.name); s.bind(2, m.rating); s.bind(3, m.category_id);
  s.bind(4, m.size_mb); s.bind(5, m.is_deleted ? 1 : 0); s.bind(6, m.file_path); s.bind(7, m.file_hash);
  s.step();
  int id = (int)sqlite3_last_insert_rowid(db_->raw());
  tx.commit();
  return id;
}

std::vector<ModRow> RepositoryDao::listVisible() {
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
