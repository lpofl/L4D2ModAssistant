#include "core/config/Settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <spdlog/spdlog.h>

/**
 * @file Settings.cpp
 * @brief Load/create application settings backed by a JSON file.
 */

// Note: use nlohmann::json directly; no alias to avoid unused warnings.

std::filesystem::path Settings::defaultSettingsPath() {
  // Resolve default settings path; Windows uses APPDATA, Linux uses HOME.
#ifdef _WIN32
  const char* appdata = std::getenv("APPDATA");
  std::filesystem::path base = appdata ? appdata : std::filesystem::current_path();
  return base / "L4D2ModAssistant" / "setting_config" / "LMA_settings.json";
#else
  const char* home = std::getenv("HOME");
  std::filesystem::path base = home ? (std::filesystem::path(home) / ".config") : std::filesystem::current_path();
  return base / "L4D2ModAssistant" / "setting_config" / "LMA_settings.json";
#endif
}

// (Removed unused defaultRepoDirectory and duplicate includes)

namespace {

// Helper for ImportAction enum
std::string importActionToString(ImportAction action) {
  switch (action) {
    case ImportAction::Cut: return "Cut";
    case ImportAction::Copy: return "Copy";
    case ImportAction::None: return "Link";
  }
  return "Cut"; // Default
}

ImportAction stringToImportAction(const std::string& str) {
  if (str == "Copy") return ImportAction::Copy;
  if (str == "Link") return ImportAction::None;
  return ImportAction::Cut; // Default
}

// Helper for AddonsAutoImportMethod enum
std::string addonsAutoImportMethodToString(AddonsAutoImportMethod method) {
  switch (method) {
    case AddonsAutoImportMethod::Cut: return "Cut";
    case AddonsAutoImportMethod::Copy: return "Copy";
    case AddonsAutoImportMethod::Link: return "Link";
  }
  return "Copy"; // Default
}

AddonsAutoImportMethod stringToAddonsAutoImportMethod(const std::string& str) {
  if (str == "Cut") return AddonsAutoImportMethod::Cut;
  if (str == "Link") return AddonsAutoImportMethod::Link;
  return AddonsAutoImportMethod::Copy; // Default
}

} // namespace

Settings Settings::loadOrCreate() {
  Settings settings;
  const std::filesystem::path defaultDbPath = std::filesystem::path("database") / "repo.db";
  const auto path = Settings::defaultSettingsPath();
  std::ifstream ifs(path);
  if (ifs.is_open()) {
    try {
      nlohmann::json j;
      ifs >> j;
      settings.repoDir = j.value("repoDir", ""); // Default to empty string
      settings.repoDbPath = defaultDbPath.string();
      // New settings
      settings.gameDirectory = j.value("gameDirectory", "");
      settings.importAction = stringToImportAction(j.value("importAction", "Cut"));
      settings.addonsAutoImportEnabled = j.value("addonsAutoImportEnabled", false);
      settings.addonsAutoImportMethod = stringToAddonsAutoImportMethod(j.value("addonsAutoImportMethod", "Copy"));
      settings.combinerMemoryWarningMb = j.value("combinerMemoryWarningMb", 2048); // Default
      settings.retainDataOnDelete = j.value("retainDataOnDelete", true); // Default
    } catch (const nlohmann::json::exception& e) {
      spdlog::error("Failed to parse settings file {}: {}", path.string(), e.what());
      // Fallback to default settings
      settings.repoDir = "";
      settings.repoDbPath = defaultDbPath.string();
      settings.gameDirectory = "";
      settings.importAction = ImportAction::Cut;
      settings.addonsAutoImportEnabled = false;
      settings.addonsAutoImportMethod = AddonsAutoImportMethod::Copy;
      settings.combinerMemoryWarningMb = 2048; // Default
      settings.retainDataOnDelete = true; // Default
      // Write back defaults to recover a broken file
      settings.save();
    }
  } else {
    spdlog::info("Settings file not found at {}, creating default.", path.string());
    settings.repoDir = "";
    settings.repoDbPath = defaultDbPath.string();
    settings.gameDirectory = "";
    settings.importAction = ImportAction::Cut;
    settings.addonsAutoImportEnabled = false;
    settings.addonsAutoImportMethod = AddonsAutoImportMethod::Copy;
    settings.combinerMemoryWarningMb = 2048; // Default
    settings.retainDataOnDelete = true; // Default
    settings.save(); // Save default settings
  }
  return settings;
}

void Settings::save() const {
  nlohmann::json j;
  j["repoDir"] = repoDir;
  // New settings
  j["gameDirectory"] = gameDirectory;
  j["importAction"] = importActionToString(importAction);
  j["addonsAutoImportEnabled"] = addonsAutoImportEnabled;
  j["addonsAutoImportMethod"] = addonsAutoImportMethodToString(addonsAutoImportMethod);
  j["combinerMemoryWarningMb"] = combinerMemoryWarningMb;
  j["retainDataOnDelete"] = retainDataOnDelete;
  const auto path = Settings::defaultSettingsPath();
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    spdlog::error("Failed to create settings directory {}: {}", path.parent_path().string(), ec.message());
  }
  std::ofstream ofs(path);
  if (ofs.is_open()) {
    ofs << j.dump(2); // Pretty print with 2 spaces
  } else {
    spdlog::error("Failed to save settings file {}", path.string());
  }
}
