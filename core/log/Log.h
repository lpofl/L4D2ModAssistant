#pragma once

#include <memory>

#include <spdlog/sinks/basic_file_sink.h>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <spdlog/spdlog.h>


/**

 * @file Log.h

 * @brief 日志初始化工具。

 */


/**

 * @brief 初始化全局日志器（幂等）。

 *

 * 若默认 logger 不存在，则设置日志级别与输出格式。

 * 同时输出到控制台和 l4d2-mod-assistant.log 文件。

 */

inline void initLogging() {

    if (!spdlog::get("multi_sink")) {

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        console_sink->set_pattern("[%H:%M:%S] [%^%l%$] %v");


        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("l4d2-mod-assistant.log", true);

        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");


        auto logger = std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{console_sink, file_sink});

        logger->set_level(spdlog::level::trace);

        logger->flush_on(spdlog::level::info);


        spdlog::set_default_logger(logger);

    }

}