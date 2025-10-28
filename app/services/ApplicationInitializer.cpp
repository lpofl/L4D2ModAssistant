// UTF-8
#include "app/services/ApplicationInitializer.h"

#include "core/db/Db.h"
#include "core/db/Migrations.h"
#include "core/log/Log.h"

std::unique_ptr<RepositoryService> ApplicationInitializer::createRepositoryService(const Settings& settings) {
  // 应用层装配：创建数据库、执行迁移、构建仓库服务
  auto db = std::make_shared<Db>(settings.repoDbPath);
  runMigrations(*db); // 执行数据库迁移，确保 Schema 版本一致
  spdlog::info("Schema ready, version {}", migrations::currentSchemaVersion(*db));
  return std::make_unique<RepositoryService>(db);
}

