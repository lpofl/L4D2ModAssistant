
cmake_minimum_required(VERSION 3.24)
project(L4D2ModManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# If building with vcpkg
if (EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(SQLite3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(L4D2ModManager
  app/main.cpp
  app/ui/MainWindow.cpp
  app/ui/MainWindow.h
  app/ui/MainWindow.ui
  core/db/Db.cpp
  core/db/Db.h
  core/db/Stmt.h
  core/db/Migrations.h
  core/repo/RepositoryDao.cpp
  core/repo/RepositoryDao.h
  core/repo/RepositoryService.cpp
  core/repo/RepositoryService.h
  core/config/Settings.cpp
  core/config/Settings.h
  core/log/Log.cpp
  core/log/Log.h
)

target_include_directories(L4D2ModManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(L4D2ModManager
  PRIVATE
    Qt6::Widgets
    SQLite::SQLite3
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# On Windows, deploy Qt plugins may be necessary (skipped here)
