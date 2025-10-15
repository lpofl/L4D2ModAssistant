
#pragma once
#include <string>
#include <filesystem>

/**
 * @file Settings.h
 * @brief 应用配置的结构体定义与加载入口。
 */

/**
 * @brief 应用启动/运行时配置。
 */
struct Settings {
  std::string repoDbPath;     ///< 仓库数据库文件路径。
  std::string repoDir;        ///< 仓库存放目录。
  std::string gameAddonsDir;  ///< 游戏 addons 目录。
  bool moveOnImport{false};   ///< 导入文件时是否移动而非复制。
  bool showDeleted{true};     ///< UI 是否展示已删除项。

  /**
   * @brief 读取配置文件，如不存在则创建默认配置并写入。
   * @return 加载后的 Settings。
   */
  static Settings loadOrCreate();

  /**
   * @brief 获取默认配置文件路径（跨平台）。
   * @return 配置文件路径。
   */
  static std::filesystem::path defaultSettingsPath();
};
