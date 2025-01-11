#include <graphics/helper.hpp>

namespace foundation {
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

    void reallocate_buffer(Context* context, daxa::CommandRecorder& cmd_recorder, daxa::TaskBuffer& task_buffer, usize new_size) {
        daxa::BufferId buffer = task_buffer.get_state().buffers[0];
        auto info = context->device.buffer_info(buffer).value();
        if(info.size < new_size) {
            context->destroy_buffer_deferred(cmd_recorder, buffer);
            LOG_INFO("{} buffer resized from {} bytes to {} bytes", info.name.c_str().data(), info.size, new_size);
            u32 old_size = s_cast<u32>(info.size);
            info.size = new_size;
            daxa::BufferId new_buffer = context->create_buffer(info);
            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = buffer,
                .dst_buffer = new_buffer,
                .src_offset = 0,
                .dst_offset = 0,
                .size = old_size,
            });

            task_buffer.set_buffers({ .buffers=std::array{new_buffer} });
        }
    }
}