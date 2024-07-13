#include <graphics/helper.hpp>

namespace Shaper {
    auto make_task_buffer(Context* context, const daxa::BufferInfo& info) -> daxa::TaskBuffer {
        return daxa::TaskBuffer{daxa::TaskBufferInfo{
            .initial_buffers = daxa::TrackedBuffers{ 
                .buffers = std::array{
                    context->create_buffer({
                        .size = info.size,
                        .allocate_info = info.allocate_info,
                        .name = info.name,
                    }),
                },
            },
            .name = std::string(info.name.c_str().data()),
        }};
    }
}