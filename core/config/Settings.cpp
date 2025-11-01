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

/**
 * @brief 将 ImportAction 枚举转换为其字符串表示。
 * @param action 枚举值。
 * @return 字符串表示。
 */
std::string importActionToString(ImportAction action) {
  switch (action) {
    case ImportAction::Cut: return "Cut";
    case ImportAction::Copy: return "Copy";
    case ImportAction::Link: return "Link";
  }
  return "Cut"; // Default
}

/**
 * @brief 将字符串转换为 ImportAction 枚举。
 * @param str 字符串值。
 * @return 枚举值。
 */
ImportAction stringToImportAction(const std::string& str) {
  if (str == "Copy") return ImportAction::Copy;
  if (str == "Link") return ImportAction::Link;
  return ImportAction::Cut; // Default
}

/**
 * @brief 将 AddonsAutoImportMethod 枚举转换为其字符串表示。
 * @param method 枚举值。
 * @return 字符串表示。
 */
std::string addonsAutoImportMethodToString(AddonsAutoImportMethod method) {
  switch (method) {
    case AddonsAutoImportMethod::Cut: return "Cut";
    case AddonsAutoImportMethod::Copy: return "Copy";
    case AddonsAutoImportMethod::Link: return "Link";
  }
  return "Copy"; // Default
}

/**
 * @brief 将字符串转换为 AddonsAutoImportMethod 枚举。
 * @param str 字符串值。
 * @return 枚举值。
 */
AddonsAutoImportMethod stringToAddonsAutoImportMethod(const std::string& str) {
  if (str == "Cut") return AddonsAutoImportMethod::Cut;
  if (str == "Link") return AddonsAutoImportMethod::Link;
  return AddonsAutoImportMethod::Copy; // Default
}

/**
 * @brief 根据给定的游戏相关根路径推导 addons 路径。
 * @param root 可以是游戏根目录、'left4dead2' 目录或 'addons' 目录的路径。
 * @return 推导出的 addons 路径。
 * @details
 * - 如果路径以 "addons" 结尾，则直接返回。
 * - 如果路径以 "left4dead2" 结尾，则附加 "/addons"。
 * - 否则，将其视为游戏根目录，并附加 "/left4dead2/addons"。
 */
static std::string deriveAddonsPathFs(const std::string& root) {
  if (root.empty()) return {};
  std::filesystem::path p = std::filesystem::weakly_canonical(std::filesystem::path(root));
  // 上述 canonical 调用可能失败；忽略异常并回退到原始路径。
  std::error_code ec;
  auto filename = p.filename().generic_string();
  for (auto& c : filename) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

  if (filename == "addons") {
    return p.string();
  }
  if (filename == "left4dead2") {
    return (p / "addons").string();
  }
  return (p / "left4dead2" / "addons").string();
}

/**
 * @brief 根据给定的 addons 路径推导 workshop 路径。
 * @param addonsPath addons 目录的路径。
 * @return 推导出的 workshop 路径 (addons/workshop)。
 */
static std::string deriveWorkshopPathFs(const std::string& addonsPath) {
  if (addonsPath.empty()) return {};
  std::filesystem::path p(addonsPath);
  return (p / "workshop").string();
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
      // 基于 gameDirectory 推导并填充 addons/workshop 路径
      settings.addonsPath = deriveAddonsPathFs(settings.gameDirectory); // 推导 addons 路径
      settings.workshopPath = deriveWorkshopPathFs(settings.addonsPath); // 推导 workshop 路径
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
      // 推导路径为空
      settings.addonsPath = {};
      settings.workshopPath = {};
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
    settings.addonsPath = {};
    settings.workshopPath = {};
    settings.save(); // Save default settings
  }
  return settings;
}

void Settings::save() const {
  nlohmann::json j;
  j["repoDir"] = repoDir;
  // New settings
  j["gameDirectory"] = gameDirectory;
  // 持久化推导出的路径，便于其它模块直接读取
  j["addonsPath"] = addonsPath;
  j["workshopPath"] = workshopPath;
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
