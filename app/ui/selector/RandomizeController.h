// UTF-8
#pragma once

#include <memory>

#include <QObject>

#include "core/random/Randomizer.h"

/**
 * 随机器控制器：封装 UI 触发到核心 Randomizer 的调用装配，
 * 避免将算法与装配细节放在界面类中。
 */
class RandomizeController : public QObject {
  Q_OBJECT
public:
  explicit RandomizeController(RepositoryService& service, QObject* parent = nullptr);

  /**
   * 使用给定配置执行随机组合，返回结果。
   * 该方法不更新 UI，仅返回数据，供上层 ViewModel/页面自行展示。
   */
  RandomizerResult randomize(const RandomizerConfig& config,
                             const RandomizerContext& ctx = RandomizerContext());

private:
  RepositoryService& service_;
};

