#pragma once

#include "pch.hpp"

namespace foundation {
    auto load_file_to_string(const std::string_view& file_path) -> std::string;
}
