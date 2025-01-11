#include "logger.hpp"

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>

namespace foundation {
    static quill::Logger* s_logger = nullptr;

    void Logger::init() {
#if defined(LOG_ON)
        quill::BackendOptions backend_options;
        quill::Backend::start(backend_options);

        quill::PatternFormatterOptions format_options("[%(time)] [%(log_level)] %(message)", "%H:%M:%S.%Qms");
        auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1");
        s_logger = quill::Frontend::create_or_get_logger("root", std::move(console_sink), format_options);
#endif
    }

    auto Logger::get_logger() -> quill::Logger* {
        return s_logger;
    }
}