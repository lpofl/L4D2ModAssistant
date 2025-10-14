
#pragma once
#include <spdlog/spdlog.h>
#include <memory>

inline void initLogging() {
    if (!spdlog::default_logger()) {
        spdlog::set_level(spdlog::level::info);
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
    }
}
