// UTF-8
#include "app/ui/selector/RandomizeController.h"

RandomizeController::RandomizeController(RepositoryService& service, QObject* parent)
    : QObject(parent), service_(service) {}

RandomizerResult RandomizeController::randomize(const RandomizerConfig& config,
                                               const RandomizerContext& ctx) {
  // 组装核心随机器并返回结果（不涉及任何 UI 元素）
  Randomizer randomizer(service_);
  return randomizer.generate(config, ctx);
}

