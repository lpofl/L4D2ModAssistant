#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/repo/RepositoryService.h"

/**
 * @file Randomizer.h
 * @brief 随机组合器的配置描述与生成结果定义。
 */

/// 单个 MOD 的使用频次提示，用于实现“低频优先”等策略。
struct RandomizerUsageHint {
  int use_count{0};
  std::optional<std::string> last_used_at;
};

/// 随机组合器的主排序策略。
enum class RandomizerPriority {
  Balanced,          ///< 综合评分与使用频次的折中策略。
  PreferLowFrequency,///< 优先选择低使用频次的 MOD。
  PreferHighRating   ///< 优先选择高评分的 MOD。
};

/// 分类与 TAG 等过滤条件。
struct RandomizerFilter {
  std::vector<int> include_category_ids;
  std::vector<int> exclude_category_ids;
  std::vector<TagDescriptor> include_tags;
  std::vector<TagDescriptor> exclude_tags;
};

/// 随机组合生成所需的完整配置。
struct RandomizerConfig {
  RandomizerFilter filter;
  std::optional<int> min_rating;
  std::optional<int> max_rating;
  RandomizerPriority priority{RandomizerPriority::Balanced};
  bool avoid_homologous{true};
  double budget_mb{2048.0};
  std::optional<std::size_t> max_mods;
  unsigned int seed{5489u};
  std::vector<int> locked_mod_ids;
  std::vector<int> excluded_mod_ids;
  std::vector<int> fixed_bundle_ids;
  std::vector<int> saved_scheme_ids;
};

/// 生成时可选的运行时上下文，用于传入动态统计数据。
struct RandomizerContext {
  std::unordered_map<int, RandomizerUsageHint> usage_hints;
};

/// 单个入选 MOD 的来源标签。
enum class RandomizerEntryFlag : unsigned {
  None = 0,
  Locked = 1u << 0,
  FromBundle = 1u << 1,
  Dependency = 1u << 2
};

/// 随机方案条目。
struct RandomizerEntry {
  int mod_id{0};
  double size_mb{0.0};
  RandomizerEntryFlag flags{RandomizerEntryFlag::None};
};

/// 随机组合器的完整输出结果。
struct RandomizerResult {
  std::vector<RandomizerEntry> entries;
  double total_size_mb{0.0};
  std::vector<int> skipped_by_conflict;
  std::vector<int> skipped_by_budget;
  std::vector<int> skipped_by_homologous;
  std::vector<int> missing_dependencies;
};

inline RandomizerEntryFlag operator|(RandomizerEntryFlag lhs, RandomizerEntryFlag rhs) {
  return static_cast<RandomizerEntryFlag>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs));
}

inline RandomizerEntryFlag& operator|=(RandomizerEntryFlag& lhs, RandomizerEntryFlag rhs) {
  lhs = lhs | rhs;
  return lhs;
}

class Randomizer {
public:
  explicit Randomizer(RepositoryService& service);

  RandomizerResult generate(const RandomizerConfig& config,
                            const RandomizerContext& context = RandomizerContext()) const;

private:
  RepositoryService& service_;
};

