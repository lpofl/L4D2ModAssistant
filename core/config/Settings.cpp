#include "core/config/Settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>

/**
 * @file Settings.cpp
 * @brief Load/create application settings backed by a JSON file.
 */

using json = nlohmann::json;

std::filesystem::path Settings::defaultSettingsPath() {
  // Resolve default settings path; Windows uses APPDATA, Linux uses HOME.
#ifdef _WIN32
  const char* appdata = std::getenv("APPDATA");
  std::filesystem::path base = appdata ? appdata : std::filesystem::current_path();
  return base / "L4D2ModAssistant" / "settings.json";
#else
  const char* home = std::getenv("HOME");
  std::filesystem::path base = home ? (std::filesystem::path(home) / ".config") : std::filesystem::current_path();
  return base / "L4D2ModAssistant" / "settings.json";
#endif
}

namespace {
std::filesystem::path defaultRepoDirectory() {
  return std::filesystem::path("database");
}
}

Settings Settings::loadOrCreate() {
  Settings s;
  auto path = defaultSettingsPath(); // Default config location
  std::filesystem::create_directories(path.parent_path());

  const auto repoDirDefault = defaultRepoDirectory();
  const std::string repoDbDefaultName = "l4d2mod.db";

  if (std::filesystem::exists(path)) { // Config file exists
    std::ifstream in(path);
    json j;
    in >> j;
    in.close();

    bool shouldRewrite = false;
    s.repoDir = j.value("repo_dir", repoDirDefault.string());
    if (s.repoDir == "repo") {
      s.repoDir = repoDirDefault.string();
      shouldRewrite = true;
    }

    auto repoDbFromConfig = j.value("repo_db_path", std::string{});
    if (!repoDbFromConfig.empty()) {
      if (repoDbFromConfig == "repo/repo.db") {
        s.repoDbPath = (repoDirDefault / repoDbDefaultName).string();
        shouldRewrite = true;
      } else {
        s.repoDbPath = repoDbFromConfig;
      }
    } else {
      s.repoDbPath = (std::filesystem::path(s.repoDir) / repoDbDefaultName).string();
      shouldRewrite = true;
    }
    s.gameAddonsDir = j.value("game_addons_dir", "left4dead2/addons");
    s.moveOnImport = j.value("move_on_import", false);
    s.showDeleted = j.value("show_deleted", true);

    if (shouldRewrite) {
      j["repo_dir"] = s.repoDir;
      j["repo_db_path"] = s.repoDbPath;
      std::ofstream out(path);
      out << j.dump(2);
    }
  } else { // Config file missing; create defaults
    s.repoDir = repoDirDefault.string();
    std::filesystem::create_directories(s.repoDir);
    s.repoDbPath = (repoDirDefault / repoDbDefaultName).string();
    s.gameAddonsDir = "left4dead2/addons";
    s.moveOnImport = false;
    s.showDeleted = true;
    json j = {
        {"repo_dir", s.repoDir},
        {"repo_db_path", s.repoDbPath},
        {"game_addons_dir", s.gameAddonsDir},
        {"move_on_import", s.moveOnImport},
        {"show_deleted", s.showDeleted},
        {"ui", {{"columns", {"name", "author", "rating", "category", "tags", "note"}}}}};
    std::ofstream out(path);
    out << j.dump(2);
  }

  if (s.repoDir.empty()) {
    s.repoDir = repoDirDefault.string();
  }
  auto repoDirPath = std::filesystem::path(s.repoDir);
  if (!repoDirPath.empty()) {
    std::filesystem::create_directories(repoDirPath);
  }
  auto dbPath = std::filesystem::path(s.repoDbPath);
  if (!dbPath.parent_path().empty()) {
    std::filesystem::create_directories(dbPath.parent_path());
  }
  return s;
}
