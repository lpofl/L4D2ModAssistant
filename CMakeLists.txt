
cmake_minimum_required(VERSION 3.24)
project(L4D2ModAssistant LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# If building with vcpkg
if(NOT DEFINED CMAKE_PREFIX_PATH)
  set(CMAKE_PREFIX_PATH "C:/Qt/6.9.3/msvc2022_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(SQLite3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(GTest CONFIG REQUIRED)

add_executable(L4D2ModAssistant
  app/main.cpp
  app/ui/MainWindow.cpp
  app/ui/MainWindow.h
  app/ui/MainWindow.ui
  app/ui/ModEditorDialog.cpp
  app/ui/ModEditorDialog.h
  core/db/Db.cpp
  core/db/Db.h
  core/db/Stmt.h
  core/db/Migrations.h
  core/repo/CategoryDao.cpp
  core/repo/CategoryDao.h
  core/repo/TagDao.cpp
  core/repo/TagDao.h
  core/repo/ModRelationDao.cpp
  core/repo/ModRelationDao.h
  core/repo/SavedSchemeDao.cpp
  core/repo/SavedSchemeDao.h
  core/repo/FixedBundleDao.cpp
  core/repo/FixedBundleDao.h
  core/repo/RepositoryDao.cpp
  core/repo/RepositoryDao.h
  core/repo/RepositoryService.cpp
  core/repo/RepositoryService.h
  core/random/Randomizer.cpp
  core/random/Randomizer.h
  core/config/AttributeOptions.cpp
  core/config/AttributeOptions.h
  core/config/Settings.cpp
  core/config/Settings.h
  core/log/Log.cpp
  core/log/Log.h
)

target_include_directories(L4D2ModAssistant PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(L4D2ModAssistant
  PRIVATE
    Qt6::Widgets
    SQLite::SQLite3
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

enable_testing()

add_executable(L4D2ModAssistantTests
  tests/SavedSchemeTests.cpp
  core/db/Db.cpp
  core/db/Db.h
  core/db/Stmt.h
  core/db/Migrations.h
  core/repo/CategoryDao.cpp
  core/repo/CategoryDao.h
  core/repo/RepositoryDao.cpp
  core/repo/RepositoryDao.h
  core/repo/TagDao.cpp
  core/repo/TagDao.h
  core/repo/ModRelationDao.cpp
  core/repo/ModRelationDao.h
  core/repo/SavedSchemeDao.cpp
  core/repo/SavedSchemeDao.h
  core/repo/FixedBundleDao.cpp
  core/repo/FixedBundleDao.h
  core/repo/RepositoryService.cpp
  core/repo/RepositoryService.h
  core/random/Randomizer.cpp
  core/random/Randomizer.h
  core/config/AttributeOptions.cpp
  core/config/AttributeOptions.h
)

target_include_directories(L4D2ModAssistantTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(L4D2ModAssistantTests
  PRIVATE
    SQLite::SQLite3
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    GTest::gtest_main
)

add_test(NAME L4D2RepoTests COMMAND L4D2ModAssistantTests)

# On Windows, deploy Qt plugins may be necessary (skipped here)
