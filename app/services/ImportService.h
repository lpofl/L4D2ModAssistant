// UTF-8
#pragma once

#include <QStringList>

#include "core/config/Settings.h"
#include "core/repo/RepositoryService.h"

/**
 * 导入服务：负责与仓库目录相关的文件处理（复制/剪切、命名去重、路径规范化）。
 * 说明：从 UI 的 MainWindow 中拆出，避免界面层承载文件系统与策略逻辑。
 */
class ImportService {
public:
  ImportService() = default;

  /**
   * 按 Settings 的导入策略，确保 MOD 文件与封面位于仓库目录中；必要时执行复制/剪切与重命名。
   * - mod.file_path / mod.cover_path 可能被此方法更新为目标仓库内的新路径（使用系统本地分隔符）。
   * - errors 输出详细的人类可读错误信息（中文）。
   */
  bool ensureModFilesInRepository(const Settings& settings, ModRow& mod, QStringList& errors) const;
};

