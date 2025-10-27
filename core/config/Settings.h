
#pragma once

#include <string>
#include <filesystem>

// Enums for settings
enum class ImportAction {
  Cut,
  Copy,
  None
};

enum class AddonsAutoImportMethod {
  Cut,
  Copy,
  Link
};

struct Settings {
  std::string repoDbPath; // Path to the repository database file
  std::string repoDir;    // Path to the MOD repository directory

  std::string gameDirectory; // Path to the game's addons directory (left4dead2/addons)

  ImportAction importAction{ImportAction::Cut}; // Default: Cut
  bool addonsAutoImportEnabled{false};
  AddonsAutoImportMethod addonsAutoImportMethod{AddonsAutoImportMethod::Copy}; // Default: Copy
  int combinerMemoryWarningMb{2048}; // Default: 2048 MB
  bool retainDataOnDelete{true}; // Default: true

  static Settings loadOrCreate();
  void save() const;
  
  /**
   * @brief 获取默认配置文件路径（跨平台）。
   * @return 配置文件路径。
   */
  static std::filesystem::path defaultSettingsPath();
};
