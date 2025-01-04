#pragma once

#include <pch.hpp>

namespace foundation {
    struct PerfomanceCounter {
        std::string_view task_name = {};
        std::string_view name = {};
    };

    struct PerfomanceCategory {
        std::string name = {};
        std::vector<PerfomanceCounter> counters = {};
        std::vector<PerfomanceCategory> performance_categories = {};
    };
}