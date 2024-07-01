#include <graphics/helper.hpp>

namespace Shaper {
    auto make_task_buffer(daxa::Device & device, u32 size, daxa::MemoryFlags memory_flag, std::string_view name) -> daxa::TaskBuffer {
        return daxa::TaskBuffer{daxa::TaskBufferInfo{
            .initial_buffers = daxa::TrackedBuffers{ 
                .buffers = std::array{
                    device.create_buffer({
                        .size = size,
                        .allocate_info = memory_flag,
                        .name = name,
                    }),
                },
            },
            .name = std::string(name),
        }};
    }
}