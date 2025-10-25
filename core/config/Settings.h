
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
  Copy,
  Move
};

struct Settings {
  std::string repoDbPath; // Path to the repository database file
  std::string repoDir;    // Path to the MOD repository directory

  std::string gameDirectory; // Path to the game installation directory

  ImportAction importAction{ImportAction::Cut}; // Default: Cut
  bool addonsAutoImportEnabled{false};
  AddonsAutoImportMethod addonsAutoImportMethod{AddonsAutoImportMethod::Copy}; // Default: Copy
  int combinerMemoryWarningMb{1024}; // Default: 1024 MB
  bool retainDataOnDelete{false}; // Default: false

  static Settings loadOrCreate();
  void save() const;
  
  /**
   * @brief 获取默认配置文件路径（跨平台）。
   * @return 配置文件路径。
   */
  static std::filesystem::path defaultSettingsPath();
};
