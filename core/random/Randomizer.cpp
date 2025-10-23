#include "core/random/Randomizer.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <random>
#include <unordered_set>

namespace {

struct ModDetail {
  ModRow row;
  std::vector<TagDescriptor> tags;
  std::unordered_set<std::string> tag_keys;
};

using RelationSet = std::unordered_map<int, std::unordered_set<int>>;

std::string makeTagKey(const TagDescriptor& tag) {
  static constexpr char kDelimiter = '\x1F';
  return tag.group + kDelimiter + tag.tag;
}

template <typename Container>
void recordUnique(Container& container, int value) {
  if (std::find(container.begin(), container.end(), value) == container.end()) {
    container.push_back(value);
  }
}

std::vector<ModRelationRow> loadAllRelations(RepositoryService& service,
                                             const std::vector<ModRow>& mods) {
  std::vector<ModRelationRow> relations;
  std::unordered_set<int> seenIds;
  relations.reserve(mods.size() * 2);
  for (const auto& mod : mods) {
    auto rows = service.listRelationsForMod(mod.id);
    for (const auto& row : rows) {
      if (seenIds.insert(row.id).second) {
        relations.push_back(row);
      }
    }
  }
  return relations;
}

RelationSet buildHomologousGraph(const std::vector<ModRelationRow>& relations) {
  RelationSet graph;
  for (const auto& rel : relations) {
    if (rel.type == "homologous") {
      graph[rel.a_mod_id].insert(rel.b_mod_id);
      graph[rel.b_mod_id].insert(rel.a_mod_id);
    }
  }
  return graph;
}

std::unordered_map<int, int> assignHomologousGroup(const RelationSet& graph,
                                                   const std::unordered_set<int>& modIds) {
  std::unordered_map<int, int> group;
  int nextGroup = 1;
  std::unordered_set<int> visited;
  for (int id : modIds) {
    if (visited.count(id) != 0) {
      continue;
    }
    auto it = graph.find(id);
    if (it == graph.end()) {
      group[id] = 0;
      visited.insert(id);
      continue;
    }
    std::queue<int> queue;
    queue.push(id);
    visited.insert(id);
    group[id] = nextGroup;
    while (!queue.empty()) {
      int current = queue.front();
      queue.pop();
      auto git = graph.find(current);
      if (git == graph.end()) {
        continue;
      }
      for (int neighbor : git->second) {
        if (visited.insert(neighbor).second) {
          group[neighbor] = nextGroup;
          queue.push(neighbor);
        }
      }
    }
    ++nextGroup;
  }
  for (int id : modIds) {
    if (group.find(id) == group.end()) {
      group[id] = 0;
    }
  }
  return group;
}

} // namespace

Randomizer::Randomizer(RepositoryService& service) : service_(service) {}

RandomizerResult Randomizer::generate(const RandomizerConfig& config,
                                      const RandomizerContext& context) const {
  RandomizerResult result;
  auto mods = service_.listVisible();
  if (mods.empty()) {
    return result;
  }

  // 构建 MOD 基础信息与 TAG 反查索引，便于后续过滤。
  std::unordered_map<int, ModDetail> details;
  details.reserve(mods.size());
  std::unordered_set<int> modIds;
  modIds.reserve(mods.size());
  for (const auto& mod : mods) {
    ModDetail detail;
    detail.row = mod;
    auto tagRows = service_.listTagsForMod(mod.id);
    detail.tags.reserve(tagRows.size());
    for (const auto& tagRow : tagRows) {
      TagDescriptor descriptor{tagRow.group_name, tagRow.name};
      detail.tags.push_back(descriptor);
      detail.tag_keys.insert(makeTagKey(descriptor));
    }
    modIds.insert(mod.id);
    details.emplace(mod.id, std::move(detail));
  }

  // 预读取所有关系，拆分为依赖图、冲突图以及同质连接图。
  auto relations = loadAllRelations(service_, mods);
RelationSet requiresGraph;
  RelationSet conflicts;
  RelationSet homologousGraph = buildHomologousGraph(relations);
  for (const auto& relation : relations) {
    if (relation.type == "requires") {
      requiresGraph[relation.a_mod_id].insert(relation.b_mod_id);
    } else if (relation.type == "conflicts") {
      conflicts[relation.a_mod_id].insert(relation.b_mod_id);
      conflicts[relation.b_mod_id].insert(relation.a_mod_id);
    }
  }
  auto homologousGroup = assignHomologousGroup(homologousGraph, modIds);

  // 预处理分类、标签等过滤维度，加速后续判定。
  std::unordered_set<int> includeCats(config.filter.include_category_ids.begin(),
                                      config.filter.include_category_ids.end());
  std::unordered_set<int> excludeCats(config.filter.exclude_category_ids.begin(),
                                      config.filter.exclude_category_ids.end());
  std::vector<std::string> includeTagKeys;
  includeTagKeys.reserve(config.filter.include_tags.size());
  for (const auto& tag : config.filter.include_tags) {
    includeTagKeys.push_back(makeTagKey(tag));
  }
  std::vector<std::string> excludeTagKeys;
  excludeTagKeys.reserve(config.filter.exclude_tags.size());
  for (const auto& tag : config.filter.exclude_tags) {
    excludeTagKeys.push_back(makeTagKey(tag));
  }

  // 用户主动剔除的 MOD 集合，后续生成时直接跳过。
  std::unordered_set<int> excluded(config.excluded_mod_ids.begin(), config.excluded_mod_ids.end());
  std::unordered_set<int> locked(config.locked_mod_ids.begin(), config.locked_mod_ids.end());
  for (int modId : config.locked_mod_ids) {
    excluded.erase(modId);
  }

  // 固定组合（bundle）视作强制包含的候选，需要记录来源标记。
  std::unordered_set<int> bundleMods;
  for (int bundleId : config.fixed_bundle_ids) {
    auto items = service_.listFixedBundleItems(bundleId);
    for (const auto& item : items) {
      bundleMods.insert(item.mod_id);
      excluded.erase(item.mod_id);
    }
  }

  std::vector<int> preselectedOrder;
  std::unordered_map<int, RandomizerEntryFlag> preselectedFlags;
  auto queuePreselected = [&](int modId, RandomizerEntryFlag flag) {
    if (modId <= 0) {
      return;
    }
    if (preselectedFlags.find(modId) == preselectedFlags.end()) {
      preselectedOrder.push_back(modId);
    }
    preselectedFlags[modId] |= flag;
    excluded.erase(modId);
  };

  // 手动锁定的 MOD 优先加入预选队列。
  for (int modId : config.locked_mod_ids) {
    locked.insert(modId);
    queuePreselected(modId, RandomizerEntryFlag::Locked);
  }

  // 保存方案可视作“组合模板”，锁定项仍需强制加入。
  for (int schemeId : config.saved_scheme_ids) {
    auto items = service_.listSavedSchemeItems(schemeId);
    for (const auto& item : items) {
      if (item.is_locked) {
        locked.insert(item.mod_id);
        queuePreselected(item.mod_id, RandomizerEntryFlag::Locked);
      } else {
        queuePreselected(item.mod_id, RandomizerEntryFlag::None);
      }
    }
  }

  for (int modId : bundleMods) {
    queuePreselected(modId, RandomizerEntryFlag::FromBundle);
  }

  // 定义过滤逻辑：分类、评分、TAG 均满足方可过筛。
  auto passesFilter = [&](const ModDetail& detail) {
    if (!includeCats.empty()) {
      if (detail.row.category_id == 0 || includeCats.count(detail.row.category_id) == 0) {
        return false;
      }
    }
    if (!excludeCats.empty() && excludeCats.count(detail.row.category_id) != 0) {
      return false;
    }
    if (config.min_rating && detail.row.rating < *config.min_rating) {
      return false;
    }
    if (config.max_rating && detail.row.rating > *config.max_rating) {
      return false;
    }
    for (const auto& key : includeTagKeys) {
      if (detail.tag_keys.count(key) == 0) {
        return false;
      }
    }
    for (const auto& key : excludeTagKeys) {
      if (detail.tag_keys.count(key) != 0) {
        return false;
      }
    }
    return true;
  };

  std::unordered_set<int> selected;
  std::unordered_set<int> usedGroups;
  double totalSize = 0.0;

  // 深度优先解析依赖闭包，保证后续一次性评估时顺序稳定。
  auto resolveDependencies = [&](int rootId) {
    std::vector<int> stack{rootId};
    std::vector<int> order;
    std::unordered_set<int> visited;
    while (!stack.empty()) {
      int current = stack.back();
      stack.pop_back();
      if (!visited.insert(current).second) {
        continue;
      }
      order.push_back(current);
      auto it = requiresGraph.find(current);
      if (it != requiresGraph.end()) {
        for (int dep : it->second) {
          stack.push_back(dep);
        }
      }
    }
    return order;
  };

  // 按“主项+依赖”整体尝试加入结果集合，并进行预算、冲突、同质校验。
  auto addWithDependencies = [&](int rootId, RandomizerEntryFlag baseFlag, bool enforceFilter) {
    auto order = resolveDependencies(rootId);
    std::vector<int> newMods;
    std::unordered_set<int> batch;
    std::unordered_set<int> localGroups;
    double addedSize = 0.0;

    for (int modId : order) {
      if (selected.count(modId) != 0) {
        continue;
      }
      if (excluded.count(modId) != 0) {
        recordUnique(result.missing_dependencies, rootId);
        return false;
      }
      auto detailIt = details.find(modId);
      if (detailIt == details.end()) {
        recordUnique(result.missing_dependencies, rootId);
        return false;
      }
      if (enforceFilter && modId == rootId && !passesFilter(detailIt->second)) {
        return false;
      }
      if (config.avoid_homologous) {
        auto groupIt = homologousGroup.find(modId);
        int groupId = groupIt != homologousGroup.end() ? groupIt->second : 0;
        if (groupId > 0) {
          if (usedGroups.count(groupId) != 0 || localGroups.count(groupId) != 0) {
            recordUnique(result.skipped_by_homologous, rootId);
            return false;
          }
          localGroups.insert(groupId);
        }
      }
      auto conflictIt = conflicts.find(modId);
      if (conflictIt != conflicts.end()) {
        for (int conflictId : conflictIt->second) {
          if (selected.count(conflictId) != 0 || batch.count(conflictId) != 0) {
            recordUnique(result.skipped_by_conflict, rootId);
            return false;
          }
        }
      }
      addedSize += detailIt->second.row.size_mb;
      batch.insert(modId);
      newMods.push_back(modId);
    }

    if (newMods.empty()) {
      return false;
    }
    if (config.max_mods && selected.size() + newMods.size() > *config.max_mods) {
      recordUnique(result.skipped_by_budget, rootId);
      return false;
    }
    if (config.budget_mb > 0 && totalSize + addedSize - config.budget_mb > 1e-6) {
      recordUnique(result.skipped_by_budget, rootId);
      return false;
    }

    for (int modId : newMods) {
      const auto& detail = details.at(modId);
      RandomizerEntryFlag flags = baseFlag;
      if (locked.count(modId) != 0) {
        flags |= RandomizerEntryFlag::Locked;
      }
      if (bundleMods.count(modId) != 0) {
        flags |= RandomizerEntryFlag::FromBundle;
      }
      if (modId != rootId) {
        flags |= RandomizerEntryFlag::Dependency;
      }
      result.entries.push_back(RandomizerEntry{modId, detail.row.size_mb, flags});
      selected.insert(modId);
      auto groupIt = homologousGroup.find(modId);
      if (config.avoid_homologous && groupIt != homologousGroup.end() && groupIt->second > 0) {
        usedGroups.insert(groupIt->second);
      }
      totalSize += detail.row.size_mb;
    }
    result.total_size_mb = totalSize;
    return true;
  };

  // 预选项（锁定 / 固定组合 / 方案种子）优先处理。
  for (int modId : preselectedOrder) {
    RandomizerEntryFlag flag = RandomizerEntryFlag::None;
    auto it = preselectedFlags.find(modId);
    if (it != preselectedFlags.end()) {
      flag = it->second;
    }
    addWithDependencies(modId, flag, false);
  }

  struct Candidate {
    const ModDetail* detail{nullptr};
    RandomizerUsageHint usage;
  };

  // 在满足过滤条件的前提下收集候选项，后续依据策略排序。
  std::vector<Candidate> candidates;
  candidates.reserve(details.size());
  for (const auto& [modId, detail] : details) {
    if (selected.count(modId) != 0) {
      continue;
    }
    if (excluded.count(modId) != 0) {
      continue;
    }
    if (!passesFilter(detail)) {
      continue;
    }
    Candidate candidate;
    candidate.detail = &detail;
    auto usageIt = context.usage_hints.find(modId);
    if (usageIt != context.usage_hints.end()) {
      candidate.usage = usageIt->second;
    }
    candidates.push_back(candidate);
  }

  std::mt19937 rng(config.seed);
  std::shuffle(candidates.begin(), candidates.end(), rng);

  // 自定义比较器：根据用户偏好在评分与频次之间调整权重，保持同分随机性。
  auto candidateLess = [&](const Candidate& lhs, const Candidate& rhs) {
    const int ratingL = lhs.detail->row.rating;
    const int ratingR = rhs.detail->row.rating;
    const int useL = lhs.usage.use_count;
    const int useR = rhs.usage.use_count;
    const auto& timeL = lhs.usage.last_used_at;
    const auto& timeR = rhs.usage.last_used_at;
    auto preferOlder = [&](const std::optional<std::string>& a, const std::optional<std::string>& b) {
      if (a && b) {
        return *a < *b;
      }
      if (a && !b) {
        return false;
      }
      if (!a && b) {
        return true;
      }
      return false;
    };
    switch (config.priority) {
      case RandomizerPriority::PreferLowFrequency:
        if (useL != useR) return useL < useR;
        if (preferOlder(timeL, timeR)) return true;
        if (preferOlder(timeR, timeL)) return false;
        if (ratingL != ratingR) return ratingL > ratingR;
        return lhs.detail->row.size_mb < rhs.detail->row.size_mb;
      case RandomizerPriority::PreferHighRating:
        if (ratingL != ratingR) return ratingL > ratingR;
        if (useL != useR) return useL < useR;
        if (preferOlder(timeL, timeR)) return true;
        if (preferOlder(timeR, timeL)) return false;
        return lhs.detail->row.size_mb < rhs.detail->row.size_mb;
      case RandomizerPriority::Balanced:
      default: {
        const int scoreL = ratingL * 100 - useL * 10;
        const int scoreR = ratingR * 100 - useR * 10;
        if (scoreL != scoreR) return scoreL > scoreR;
        if (useL != useR) return useL < useR;
        if (preferOlder(timeL, timeR)) return true;
        if (preferOlder(timeR, timeL)) return false;
        if (ratingL != ratingR) return ratingL > ratingR;
        return lhs.detail->row.size_mb < rhs.detail->row.size_mb;
      }
    }
  };

  std::stable_sort(candidates.begin(), candidates.end(), candidateLess);

  // 依序尝试加入候选项，直到命中预算或数量上限。
  for (const auto& candidate : candidates) {
    if (config.max_mods && selected.size() >= *config.max_mods) {
      break;
    }
    if (selected.count(candidate.detail->row.id) != 0) {
      continue;
    }
    addWithDependencies(candidate.detail->row.id, RandomizerEntryFlag::None, true);
  }

  return result;
}
