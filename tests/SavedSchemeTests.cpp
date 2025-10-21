#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/db/Db.h"
#include "core/db/Migrations.h"
#include "core/repo/FixedBundleDao.h"
#include "core/repo/RepositoryDao.h"
#include "core/repo/RepositoryService.h"
#include "core/repo/SavedSchemeDao.h"

namespace {

std::shared_ptr<Db> createTestDb() {
  auto db = std::make_shared<Db>(":memory:");
  runMigrations(*db);
  return db;
}

int insertTestMod(RepositoryDao& repo, const std::string& name, const std::string& hash) {
  ModRow mod;
  mod.name = name;
  mod.rating = 5;
  mod.size_mb = 100.0;
  mod.file_hash = hash;
  return repo.insertMod(mod);
}

}  // namespace

TEST(SavedSchemeDaoTest, CreateAndQueryScheme) {
  auto db = createTestDb();
  RepositoryDao repo(db);
  SavedSchemeDao dao(db);

  const auto baseline = dao.listAll();
  const int modId = insertTestMod(repo, "Demo Mod", "hash-scheme-1");
  const int schemeId = dao.insert("Scheme-A", 512.0);
  dao.addItem({schemeId, modId, true});

  auto schemes = dao.listAll();
  ASSERT_EQ(schemes.size(), baseline.size() + 1);
  const auto it =
      std::find_if(schemes.begin(), schemes.end(), [schemeId](const auto& row) { return row.id == schemeId; });
  ASSERT_NE(it, schemes.end());
  EXPECT_EQ(it->name, "Scheme-A");

  auto fetched = dao.findById(schemeId);
  ASSERT_TRUE(fetched.has_value());
  EXPECT_DOUBLE_EQ(fetched->budget_mb, 512.0);

  auto items = dao.listItems(schemeId);
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].mod_id, modId);
  EXPECT_TRUE(items[0].is_locked);

  dao.updateName(schemeId, "Scheme-B");
  dao.updateBudget(schemeId, 256.0);

  fetched = dao.findById(schemeId);
  ASSERT_TRUE(fetched.has_value());
  EXPECT_EQ(fetched->name, "Scheme-B");
  EXPECT_DOUBLE_EQ(fetched->budget_mb, 256.0);

  dao.removeItem(schemeId, modId);
  EXPECT_TRUE(dao.listItems(schemeId).empty());
}

TEST(FixedBundleDaoTest, ManageBundleLifecycle) {
  auto db = createTestDb();
  RepositoryDao repo(db);
  FixedBundleDao dao(db);

  const int mod1 = insertTestMod(repo, "Bundle Mod 1", "hash-bundle-1");
  const int mod2 = insertTestMod(repo, "Bundle Mod 2", "hash-bundle-2");

  const int bundleId = dao.insertBundle("Bundle-A", std::string("初始备注"));
  dao.addItem(bundleId, mod1);
  dao.addItem(bundleId, mod2);

  auto bundles = dao.listBundles();
  ASSERT_EQ(bundles.size(), 1u);
  EXPECT_EQ(bundles[0].name, "Bundle-A");
  ASSERT_TRUE(bundles[0].note.has_value());

  auto items = dao.listItems(bundleId);
  ASSERT_EQ(items.size(), 2u);

  dao.updateBundle(bundleId, "Bundle-B", std::nullopt);
  dao.clearItems(bundleId);
  dao.addItem(bundleId, mod2);

  bundles = dao.listBundles();
  EXPECT_EQ(bundles[0].name, "Bundle-B");
  EXPECT_FALSE(bundles[0].note.has_value());
  items = dao.listItems(bundleId);
  ASSERT_EQ(items.size(), 1u);
  EXPECT_EQ(items[0].mod_id, mod2);

  dao.deleteBundle(bundleId);
  EXPECT_TRUE(dao.listBundles().empty());
}

TEST(RepositoryServiceTest, CreateBundlesAndSchemes) {
  auto db = createTestDb();
  RepositoryService service(db);

  ModRow modA;
  modA.name = "ServiceModA";
  modA.rating = 4;
  modA.size_mb = 64.0;
  modA.file_hash = "hash-service-1";

  ModRow modB;
  modB.name = "ServiceModB";
  modB.rating = 5;
  modB.size_mb = 80.0;
  modB.file_hash = "hash-service-2";

  const int modAId = service.createModWithTags(modA, {{"Anime", "VRC"}});
  const int modBId = service.createModWithTags(modB, {{"Anime", "Arknights"}});

  const int bundleId = service.createFixedBundle("固定搭配-A", {modAId, modBId}, std::string("常用组合"));
  auto bundles = service.listFixedBundles();
  ASSERT_EQ(bundles.size(), 1u);
  EXPECT_EQ(bundles[0].name, "固定搭配-A");

  auto bundleItems = service.listFixedBundleItems(bundleId);
  EXPECT_EQ(bundleItems.size(), 2u);

  SavedSchemeItemRow schemeItem{0, modAId, true};
  const int schemeId = service.createSavedScheme("方案-A", 256.0, {schemeItem});
  auto schemes = service.listSavedSchemes();
  auto schemeIt =
      std::find_if(schemes.begin(), schemes.end(), [schemeId](const auto& row) { return row.id == schemeId; });
  ASSERT_NE(schemeIt, schemes.end());
  EXPECT_EQ(schemeIt->name, "方案-A");

  auto schemeItems = service.listSavedSchemeItems(schemeId);
  ASSERT_EQ(schemeItems.size(), 1u);
  EXPECT_TRUE(schemeItems[0].is_locked);
  EXPECT_EQ(schemeItems[0].mod_id, modAId);

  service.updateFixedBundle(bundleId, "固定搭配-B", {modBId}, std::nullopt);
  service.updateSavedSchemeItems(schemeId, {SavedSchemeItemRow{schemeId, modBId, false}});

  bundles = service.listFixedBundles();
  EXPECT_EQ(bundles[0].name, "固定搭配-B");
  bundleItems = service.listFixedBundleItems(bundleId);
  ASSERT_EQ(bundleItems.size(), 1u);
  EXPECT_EQ(bundleItems[0].mod_id, modBId);

  schemeItems = service.listSavedSchemeItems(schemeId);
  ASSERT_EQ(schemeItems.size(), 1u);
  EXPECT_FALSE(schemeItems[0].is_locked);
  EXPECT_EQ(schemeItems[0].mod_id, modBId);

  service.deleteSavedScheme(schemeId);
  schemes = service.listSavedSchemes();
  auto deletedIt =
      std::find_if(schemes.begin(), schemes.end(), [schemeId](const auto& row) { return row.id == schemeId; });
  EXPECT_EQ(deletedIt, schemes.end());
}
