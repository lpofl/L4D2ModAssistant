#include "core/config/AttributeOptions.h"

#include <filesystem>
#include <fstream>
#include <optional>

#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;

std::optional<std::filesystem::path> locateConfigFile(const std::string& filename) {
  auto cursor = std::filesystem::current_path();
  for (int depth = 0; depth < 3 && !cursor.empty(); ++depth) {
    const auto inSettings = cursor / "setting_config" / filename;
    if (std::filesystem::exists(inSettings) && std::filesystem::is_regular_file(inSettings)) {
      return inSettings;
    }
    const auto direct = cursor / filename;
    if (std::filesystem::exists(direct) && std::filesystem::is_regular_file(direct)) {
      return direct;
    }
    cursor = cursor.parent_path();
  }
  return std::nullopt;
}

void populateFromArray(const json& node, std::vector<std::string>& target) {
  if (!node.is_array()) {
    return;
  }
  std::vector<std::string> values;
  values.reserve(node.size());
  for (const auto& item : node) {
    if (item.is_string()) {
      values.push_back(item.get<std::string>());
    }
  }
  if (!values.empty()) {
    target = std::move(values);
  }
}

} // namespace

namespace config {

ModAttributeOptions loadModAttributeOptions() {
  ModAttributeOptions options{
      /*integrity=*/{"健全", "非健全"},
      /*stability=*/{"不稳定", "高面数等风险", "安全稳定", "未知"},
      /*acquisition=*/{"免费", "付费", "定制"},
  };

  constexpr auto kFilename = "mod_attributes.json";
  auto path = locateConfigFile(kFilename);
  if (!path) {
    return options;
  }

  std::ifstream in(*path);
  if (!in.is_open()) {
    return options;
  }

  try {
    json data;
    in >> data;
    auto attributesIt = data.find("attributes");
    if (attributesIt != data.end() && attributesIt->is_object()) {
      if (auto integrityIt = attributesIt->find("integrity"); integrityIt != attributesIt->end()) {
        populateFromArray(*integrityIt, options.integrity);
      }
      if (auto stabilityIt = attributesIt->find("stability"); stabilityIt != attributesIt->end()) {
        populateFromArray(*stabilityIt, options.stability);
      }
      if (auto acquisitionIt = attributesIt->find("acquisition"); acquisitionIt != attributesIt->end()) {
        populateFromArray(*acquisitionIt, options.acquisition);
      }
    }
  } catch (const std::exception&) {
    // Ignore and keep defaults.
  }

  return options;
}

} // namespace config

