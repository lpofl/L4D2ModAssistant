
#pragma once
#include <string>
#include <filesystem>

struct Settings {
  std::string repoDbPath;
  std::string repoDir;
  std::string gameAddonsDir;
  bool moveOnImport{false};
  bool showDeleted{true};

  static Settings loadOrCreate();
  static std::filesystem::path defaultSettingsPath();
};
