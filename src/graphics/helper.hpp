#pragma once

#include <pch.hpp>

namespace Shaper {
    auto make_task_buffer(daxa::Device & device, const daxa::BufferInfo& info) -> daxa::TaskBuffer;
}