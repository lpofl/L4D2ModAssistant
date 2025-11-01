
#pragma once

#include <string>
#include <filesystem>

/**
 * @brief 定义导入操作的类型。
 */
enum class ImportAction {
  Cut,  ///< 剪切文件（移动）。
  Copy, ///< 复制文件。
  Link  ///< 创建符号链接。
};

/**
 * @brief 定义 addons 目录自动导入的方式。
 */
enum class AddonsAutoImportMethod {
  Cut,  ///< 剪切文件（移动）。
  Copy, ///< 复制文件。
  Link  ///< 创建符号链接。
};

/**
 * @brief 存储应用程序的所有配置项。
 */
struct Settings {
  std::string repoDbPath; ///< MOD 仓库数据库文件路径。
  std::string repoDir;    ///< MOD 仓库根目录路径。

  std::string gameDirectory; ///< 游戏根目录路径。

  /**
   * @brief 游戏 addons 目录的绝对路径。
   * @details 基于游戏目录推导得到并持久化。其它模块可直接从 Settings 读取以避免重复推导。
   */
  std::string addonsPath;
  /**
   * @brief 游戏 workshop 目录（addons/workshop）的绝对路径。
   * @details 基于游戏目录推导得到并持久化。
   */
  std::string workshopPath;

  ImportAction importAction{ImportAction::Cut}; ///< 默认导入操作。
  bool addonsAutoImportEnabled{false}; ///< 是否启用 addons 目录自动导入。
  AddonsAutoImportMethod addonsAutoImportMethod{AddonsAutoImportMethod::Copy}; ///< addons 目录自动导入方式。
  int combinerMemoryWarningMb{2048}; ///< MOD 合并器内存警告阈值（MB）。
  bool retainDataOnDelete{true}; ///< 删除 MOD 时是否保留元数据。

  /**
   * @brief 从默认路径加载配置，若文件不存在则创建。
   * @return 加载或创建的 Settings 对象。
   */
  static Settings loadOrCreate();

  /**
   * @brief 将当前配置保存到默认路径。
   */
  void save() const;
  
  /**
   * @brief 获取默认配置文件路径（跨平台）。
   * @return 配置文件路径。
   */
  static std::filesystem::path defaultSettingsPath();
};
