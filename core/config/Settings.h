
#pragma once

#include <string>
#include <filesystem>

// Enums for settings
enum class ImportAction {
  Cut,
  Copy,
  Link
};

enum class AddonsAutoImportMethod {
  Cut,
  Copy,
  Link
};

struct Settings {
  std::string repoDbPath; // Path to the repository database file
  std::string repoDir;    // Path to the MOD repository directory

  std::string gameDirectory; // Path to the game's root directory

  // 新增字段：基于游戏目录推导得到并持久化的路径
  // - addonsPath: 指向游戏的 addons 目录绝对路径
  // - workshopPath: 指向 addons/workshop 目录绝对路径
  // 说明：其它模块可直接从 Settings 读取这两个字段以避免重复推导。
  std::string addonsPath;   // 游戏 addons 目录
  std::string workshopPath; // 游戏 workshop 目录（位于 addons/workshop）

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
