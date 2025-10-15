
#pragma once
#include <spdlog/spdlog.h>
#include <memory>

/**
 * @file Log.h
 * @brief 日志初始化工具。
 */

/**
 * @brief 初始化全局日志器（幂等）。
 *
 * 若默认 logger 不存在，则设置日志级别与输出格式。
 */
inline void initLogging() {
    if (!spdlog::default_logger()) {
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    }
}
