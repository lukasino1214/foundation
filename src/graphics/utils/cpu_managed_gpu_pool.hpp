#pragma once

#include <graphics/context.hpp>
#include <graphics/helper.hpp>
#include <pch.hpp>

namespace foundation {
    template<typename T>
    struct CPUManagedGPUPool {
        struct Handle { u32 index = 0; };

        CPUManagedGPUPool(Context* _context, const std::string_view& name) : context{_context} {
            task_buffer = make_task_buffer(context, {
                sizeof(T),
                daxa::MemoryFlagBits::DEDICATED_MEMORY,
                name
            });
        }

        ~CPUManagedGPUPool() {
            context->destroy_buffer(task_buffer.get_state().buffers[0]);
        }

        auto allocate_handle() -> Handle {
            return Handle { .index = ammount_of_handles++ };
        }

        void update_buffer(const daxa::TaskInterface& task_interface) {
            auto& cmd_recorder = task_interface.recorder;
            daxa::BufferId buffer = task_buffer.get_state().buffers[0];
            auto info = context->device.buffer_info(buffer).value();
            u32 new_size = ammount_of_handles * sizeof(T);
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

        auto update_handle(const daxa::TaskInterface& task_interface, const Handle& handle, const T& data) -> bool {
            // cope remove this
            auto alloc_optional = task_interface.allocator->allocate_fill(data);
            if(!alloc_optional.has_value()) {
                return false;
            }

            auto alloc = alloc_optional.value();

            task_interface.recorder.copy_buffer_to_buffer({
                .src_buffer = task_interface.allocator->buffer(),
                .dst_buffer = task_buffer.get_state().buffers[0],
                .src_offset = alloc.buffer_offset,
                .dst_offset = handle.index * sizeof(T),
                .size = sizeof(T),
            });

            return true;
        }

        daxa::TaskBuffer task_buffer = {};
        std::vector<Handle> free_list = {};
        u32 ammount_of_handles = {};
        Context* context = {};
    };
} 