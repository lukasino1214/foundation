#pragma once

#include "graphics/context.hpp"
#include <pch.hpp>

namespace foundation {
    auto make_task_buffer(Context* context, const daxa::BufferInfo& info) -> daxa::TaskBuffer;
}