#pragma once

#include <pch.hpp>

namespace Shaper {
    auto make_task_buffer(daxa::Device & device, u32 size, daxa::MemoryFlags memory_flag, std::string_view name) -> daxa::TaskBuffer;
}