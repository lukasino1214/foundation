#pragma once

#include "graphics/context.hpp"
#include <pch.hpp>

namespace foundation {
    auto make_task_buffer(Context* context, const daxa::BufferInfo& info) -> daxa::TaskBuffer;
    void reallocate_buffer(Context* context, daxa::CommandRecorder& cmd_recorder, daxa::TaskBuffer& task_buffer, usize new_size);

    template<typename T>
    inline void reallocate_buffer(Context* context, daxa::CommandRecorder& cmd_recorder, daxa::TaskBuffer& task_buffer, usize new_size, const std::function<T(const daxa::BufferId&)>& lambda) {
        daxa::BufferId buffer = task_buffer.get_state().buffers[0];
        auto info = context->device.buffer_info(buffer).value();
        if(info.size < new_size) {
            context->destroy_buffer_deferred(cmd_recorder, buffer);
            LOG_INFO("{} buffer resized from {} bytes to {} bytes", info.name.c_str().data(), info.size, new_size);
            info.size = new_size;
            daxa::BufferId new_buffer = context->create_buffer(info);

            auto data = lambda(new_buffer);

            daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = sizeof(decltype(data)),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = std::string{"staging"} + std::string{info.name.c_str().data()}
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            std::memcpy(context->device.buffer_host_address(staging_buffer).value(), &data, sizeof(decltype(data)));
            cmd_recorder.copy_buffer_to_buffer({
                .src_buffer = staging_buffer,
                .dst_buffer = new_buffer,
                .src_offset = 0,
                .dst_offset = 0,
                .size = sizeof(decltype(data)),
            });

            task_buffer.set_buffers({ .buffers=std::array{new_buffer} });
        }
    }
}