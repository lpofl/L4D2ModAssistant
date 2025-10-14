
#include "core/config/Settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>

using json = nlohmann::json;

std::filesystem::path Settings::defaultSettingsPath() {
#ifdef _WIN32
  const char* appdata = std::getenv("APPDATA");
  std::filesystem::path base = appdata ? appdata : std::filesystem::current_path();
  return base / "L4D2ModManager" / "settings.json";
#else
  const char* home = std::getenv("HOME");
  std::filesystem::path base = home ? (std::filesystem::path(home) / ".config") : std::filesystem::current_path();
  return base / "L4D2ModManager" / "settings.json";
#endif
}

Settings Settings::loadOrCreate() {
  Settings s;
  auto path = defaultSettingsPath();
  std::filesystem::create_directories(path.parent_path());

  if (std::filesystem::exists(path)) {
    std::ifstream in(path);
    json j; in >> j;
    s.repoDir = j.value("repo_dir", "repo");
    s.repoDbPath = (std::filesystem::path(s.repoDir) / "repo.db").string();
    s.gameAddonsDir = j.value("game_addons_dir", "left4dead2/addons");
    s.moveOnImport = j.value("move_on_import", false);
    s.showDeleted = j.value("show_deleted", true);
  } else {
    s.repoDir = "repo";
    std::filesystem::create_directories(s.repoDir);
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
