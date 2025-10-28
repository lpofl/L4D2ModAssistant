// UTF-8
#pragma once

#include <memory>

#include "core/config/Settings.h"
#include "core/repo/RepositoryService.h"

/**
 * 应用初始化与依赖装配服务。
 * 职责：根据当前配置创建数据库连接、执行数据库迁移并装配 RepositoryService。
 * 注意：仅做应用层装配，不承载任何 UI 逻辑。
 */
class ApplicationInitializer {
public:
  /**
   * 根据 Settings 创建并返回仓库服务实例。
   * - 负责新建 Db，执行迁移，构建 RepositoryService。
   * - 如需扩展，可在此集中添加更多应用启动装配逻辑。
   */
  static std::unique_ptr<RepositoryService> createRepositoryService(const Settings& settings);
};

