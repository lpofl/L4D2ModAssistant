/**
 * @file AttributeOptions.cpp
 * @brief 实现从 JSON 配置文件加载 MOD 属性选项。
 */

#include "core/config/AttributeOptions.h"

#include <filesystem>
#include <fstream>
#include <optional>

#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;

/**
 * @brief 通过从当前目录向上搜索来定位配置文件。
 * @param filename 要查找的文件名。
 * @return 如果找到文件，则返回完整路径；否则返回 std::nullopt。
 * @note 在 `setting_config/` 子目录和目录本身中搜索，最多向上搜索 3 层。
 */
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

/**
 * @brief 从 JSON 数组填充字符串向量。
 * @param node JSON 节点，预期为字符串数组。
 * @param[out] target 用于填充 JSON 数组值的向量。
 */
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

