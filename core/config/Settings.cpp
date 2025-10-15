
#include "core/config/Settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>

/**
 * @file Settings.cpp
 * @brief 设置的加载/创建实现，基于 JSON 文件存储。
 */

using json = nlohmann::json;

std::filesystem::path Settings::defaultSettingsPath() {
  //编译时判断系统类型，Windows 使用 APPDATA 目录，Linux 使用 HOME 目录
#ifdef _WIN32
  const char* appdata = std::getenv("APPDATA");//获取环境变量APPDATA
  std::filesystem::path base = appdata ? appdata : std::filesystem::current_path();
  return base / "L4D2ModManager" / "settings.json";
#else
  const char* home = std::getenv("HOME");//获取环境变量HOME
  std::filesystem::path base = home ? (std::filesystem::path(home) / ".config") : std::filesystem::current_path();
  return base / "L4D2ModManager" / "settings.json";
#endif
}

Settings Settings::loadOrCreate() {
  Settings s;
  auto path = defaultSettingsPath();//获取默认配置文件路径
  std::filesystem::create_directories(path.parent_path());

  if (std::filesystem::exists(path)) {//如果配置文件存在
    std::ifstream in(path);//读取配置文件
    json j; in >> j;//解析配置文件
    s.repoDir = j.value("repo_dir", "repo");
    s.repoDbPath = (std::filesystem::path(s.repoDir) / "repo.db").string();//获取仓库数据库路径
    s.gameAddonsDir = j.value("game_addons_dir", "left4dead2/addons");//获取游戏addons目录
    s.moveOnImport = j.value("move_on_import", false);//获取是否移动文件
    s.showDeleted = j.value("show_deleted", true);//获取是否显示已删除项
  } else {//如果配置文件不存在
    s.repoDir = "repo";
    std::filesystem::create_directories(s.repoDir);//创建仓库目录
    s.repoDbPath = (std::filesystem::path(s.repoDir) / "repo.db").string();
    s.gameAddonsDir = "left4dead2/addons";
    s.moveOnImport = false;
    s.showDeleted = true;
    json j = {
      {"repo_dir", s.repoDir},
      {"game_addons_dir", s.gameAddonsDir},
      {"move_on_import", s.moveOnImport},
      {"show_deleted", s.showDeleted},
      {"ui", {{"columns", {"name","author","rating","category","tags","note"}}}}
    };
    std::ofstream out(path); out << j.dump(2);
  }
  return s;
}
