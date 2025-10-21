#pragma once

#include <string>
#include <vector>

/**
 * @file AttributeOptions.h
 * @brief Accessor for MOD attribute option lists configured via JSON.
 */

namespace config {

struct ModAttributeOptions {
  std::vector<std::string> integrity;
  std::vector<std::string> stability;
  std::vector<std::string> acquisition;
};

/**
 * @brief Load MOD attribute options from configuration.
 * @return Parsed options; falls back to sensible defaults when config is missing/invalid.
 */
ModAttributeOptions loadModAttributeOptions();

} // namespace config

