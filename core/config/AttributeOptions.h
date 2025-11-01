#pragma once

#include <string>
#include <vector>

/**
 * @file AttributeOptions.h
 * @brief Accessor for MOD attribute option lists configured via JSON.
 */

namespace config {

/**
 * @brief 定义各种 MOD 属性的选项列表。
 */
struct ModAttributeOptions {
  std::vector<std::string> integrity;   ///< 完整性属性的选项。
  std::vector<std::string> stability;   ///< 稳定性属性的选项。
  std::vector<std::string> acquisition; ///< 获取方式属性的选项。
};

/**
 * @brief Load MOD attribute options from configuration.
 * @return Parsed options; falls back to sensible defaults when config is missing/invalid.
 */
ModAttributeOptions loadModAttributeOptions();

} // namespace config

