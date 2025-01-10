#pragma once

#include <quill/Logger.h>
#include <quill/LogMacros.h>

#define LOG_ERROR(fmt, ...) QUILL_LOG_ERROR(Logger::get_logger(), fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) QUILL_LOG_WARNING(Logger::get_logger(), fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) QUILL_LOG_INFO(Logger::get_logger(), fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) QUILL_LOG_DEBUG(Logger::get_logger(), fmt, ##__VA_ARGS__)

namespace foundation {
    struct Logger {
        static void init();

        static auto get_logger() -> quill::Logger*;
    };
}