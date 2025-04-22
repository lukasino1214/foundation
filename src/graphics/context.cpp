#include "context.hpp"
#include <daxa/device.hpp>
#include <graphics/helper.hpp>

namespace foundation {
    ShaderDebugDrawContext::ShaderDebugDrawContext(Context* _context) : context{_context} {
        PROFILE_SCOPE;
        usize size = sizeof(ShaderDebugBufferHead);
        size += sizeof(ShaderDebugEntityOOBDraw) * entity_oob_draws.max_draws;
        size += sizeof(ShaderDebugAABBDraw) * aabb_draws.max_draws;
        size += sizeof(ShaderDebugCircleDraw) * circle_draws.max_draws;
        size += sizeof(ShaderDebugLineDraw) * line_draws.max_draws;

        this->buffer = context->create_buffer(daxa::BufferInfo {
            .size = size,
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "DebugDrawContext buffer"
        });
    }

    ShaderDebugDrawContext::~ShaderDebugDrawContext() {
        context->destroy_buffer(this->buffer);
    }

    void ShaderDebugDrawContext::update_debug_buffer(daxa::Device& device, daxa::CommandRecorder& recorder, daxa::TransferMemoryPool& allocator) {
        u32 memory_offset = sizeof(ShaderDebugBufferHead);
        auto head = ShaderDebugBufferHead {};

        auto updade_draws = [&](DrawIndirectStruct& gpu_draw, auto& cpu_draw, u64& buffer_ptr) {
            const u32 offset = memory_offset;
            memory_offset += cpu_draw.max_draws * sizeof(decltype(cpu_draw.cpu_debug_draws[0]));

            gpu_draw = DrawIndirectStruct {
                .vertex_count = cpu_draw.vertices,
                .instance_count = std::min(static_cast<u32>(cpu_draw.cpu_debug_draws.size()), cpu_draw.max_draws),
                .first_vertex = 0,
                .first_instance = 0
            };

            buffer_ptr = device.buffer_device_address(buffer).value() + offset;
        };

        updade_draws(head.entity_oob_draw_indirect_info, entity_oob_draws, head.entity_oob_draws);
        updade_draws(head.aabb_draw_indirect_info, aabb_draws, head.aabb_draws);
        updade_draws(head.circle_draw_indirect_info, circle_draws, head.circle_draws);
        updade_draws(head.line_draw_indirect_info, line_draws, head.line_draws);

        auto alloc = allocator.allocate_fill(head).value();
        recorder.copy_buffer_to_buffer({
            .src_buffer = allocator.buffer(),
            .dst_buffer = buffer,
            .src_offset = alloc.buffer_offset,
            .dst_offset = 0,
            .size = sizeof(ShaderDebugBufferHead),
        });

        auto upload_draws = [&](const DrawIndirectStruct& gpu_draw, auto& cpu_draw, const u64& buffer_ptr) {
            const u32 upload_size = sizeof(decltype(cpu_draw.cpu_debug_draws[0])) * gpu_draw.instance_count;
            if (upload_size > 0) {
                usize offset = buffer_ptr - device.buffer_device_address(buffer).value();
                auto draws_allocation = allocator.allocate(upload_size,4).value();
                std::memcpy(draws_allocation.host_address, cpu_draw.cpu_debug_draws.data(), upload_size);
                recorder.copy_buffer_to_buffer({
                    .src_buffer = allocator.buffer(),
                    .dst_buffer = buffer,
                    .src_offset = draws_allocation.buffer_offset,
                    .dst_offset = offset,
                    .size = upload_size,
                });
                cpu_draw.cpu_debug_draws.clear();
            }
        };

        upload_draws(head.entity_oob_draw_indirect_info, entity_oob_draws, head.entity_oob_draws);
        upload_draws(head.aabb_draw_indirect_info, aabb_draws, head.aabb_draws);
        upload_draws(head.circle_draw_indirect_info, circle_draws, head.circle_draws);
        upload_draws(head.line_draw_indirect_info, line_draws, head.line_draws);
    }

    Context::Context(const NativeWIndow &window)
        : instance{daxa::create_instance({})},
            device{instance.create_device(daxa::DeviceInfo{
                // .enable_buffer_device_address_capture_replay = true,
                .selector = [](daxa::DeviceProperties const & properties) -> i32 {
                    i32 score = 0;
                    switch (properties.device_type)
                    {
                    case daxa::DeviceType::DISCRETE_GPU: score += 10000; break;
                    // case daxa::DeviceType::VIRTUAL_GPU: score += 1000; break;
                    // case daxa::DeviceType::INTEGRATED_GPU: score += 100; break;
                    default: break;
                    }
                    return score;
                },
                .flags = daxa::DeviceFlagBits::BUFFER_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT | daxa::DeviceFlagBits::SHADER_ATOMIC64 | daxa::DeviceFlagBits::IMAGE_ATOMIC64 | daxa::DeviceFlagBits::MESH_SHADER,
                .max_allowed_buffers = 100000,
                .name = "my device",
            })},
            swapchain{window.create_swapchain(this->device)},
            pipeline_manager{daxa::PipelineManagerInfo{
                .device = device,
                .shader_compile_options = {
                    .root_paths = {
                        DAXA_SHADER_INCLUDE_DIR,
                        "./",
                        ""
                    },
                    .write_out_preprocessed_code = "assets/cache/preproc",
                    .write_out_shader_binary = "assets/cache/spv_raw",
                    .spirv_cache_folder = "assets/cache/spv",
                    .language = daxa::ShaderLanguage::GLSL,
                    .enable_debug_info = true,
                },
                .name = "pipeline_manager",
            }},
            transient_mem{daxa::TransferMemoryPoolInfo{
                .device = this->device,
                .capacity = 4096,
                .name = "transient memory pool",
            }},
            shader_globals{}, 
            shader_globals_buffer{make_task_buffer(this, {
                            .size = s_cast<u32>(sizeof(ShaderGlobals)),
                            .name = "globals",
                        })},
            gpu_metric_pool{std::make_unique<GPUMetricPool>(device)},
            shader_debug_draw_context{this} {
        shader_globals.debug = device.buffer_device_address(shader_debug_draw_context.buffer).value();
    }

    Context::~Context() {
        if(!shader_globals_buffer.get_state().buffers[0].is_empty()) {
            destroy_buffer(shader_globals_buffer.get_state().buffers[0]);
        }

        for(auto& [key, value] : samplers) {
            if(!value.is_empty()) {
                device.destroy_sampler(value);
            }
        }
    }

    auto Context::get_sampler(const daxa::SamplerInfo& info) -> daxa::SamplerId {
        auto find = samplers.find(info);
        if(find != samplers.end()) {
            return find->second;
        } else {
            std::lock_guard<std::mutex> lock{*resource_mutex};
            daxa::SamplerId sampler_id = device.create_sampler(info);
            samplers[info] = sampler_id;
            return sampler_id;
        }
    }

    auto Context::create_image(const daxa::ImageInfo& info) -> daxa::ImageId {
        daxa::ImageId resource = device.create_image(info);
        usize size = device.image_memory_requirements(info).size;
        image_memory_usage += size;
        return resource;
    }

    void Context::destroy_image(const daxa::ImageId& id) {
        usize size = device.image_memory_requirements(device.image_info(id).value()).size;
        image_memory_usage -= size;
        device.destroy_image(id);
    }

    void Context::destroy_image_deferred(daxa::CommandRecorder& cmd, const daxa::ImageId& id) {
        usize size = device.image_memory_requirements(device.image_info(id).value()).size;
        image_memory_usage -= size;
        cmd.destroy_image_deferred(id);
    }
    
    auto Context::create_buffer(const daxa::BufferInfo& info) -> daxa::BufferId {
        daxa::BufferId resource = device.create_buffer(info);
        usize size = device.buffer_memory_requirements(info).size;
        buffer_memory_usage += size;
        return resource;
    }

    void Context::destroy_buffer(const daxa::BufferId& id) {
        usize size = device.buffer_info(id).value().size;
        buffer_memory_usage -= size;
        device.destroy_buffer(id);
    }

    void Context::destroy_buffer_deferred(daxa::CommandRecorder& cmd, const daxa::BufferId& id) {
        usize size = device.buffer_info(id).value().size;
        buffer_memory_usage -= size;
        cmd.destroy_buffer_deferred(id);
    }

    void Context::update_shader_globals(ControlledCamera3D& main_camera, ControlledCamera3D& observer_camera, const glm::uvec2& size) {
        PROFILE_SCOPE_NAMED(update_shader_globals);

        shader_globals.render_target_size = { size.x, size.y };
        shader_globals.render_target_size_inv = {
            1.0f / s_cast<f32>(shader_globals.render_target_size.x),
            1.0f / s_cast<f32>(shader_globals.render_target_size.y),
        };
        shader_globals.next_lower_po2_render_target_size.x = find_next_lower_po2(shader_globals.render_target_size.x);
        shader_globals.next_lower_po2_render_target_size.y = find_next_lower_po2(shader_globals.render_target_size.y);
        shader_globals.next_lower_po2_render_target_size_inv = {
            1.0f / s_cast<f32>(shader_globals.next_lower_po2_render_target_size.x),
            1.0f / s_cast<f32>(shader_globals.next_lower_po2_render_target_size.y),
        };

        auto set_camera_info = [&](ControlledCamera3D& camera) -> CameraInfo {
            camera.camera.resize(s_cast<i32>(size.x), s_cast<i32>(size.y));
            CameraInfo camera_info {};
            glm::mat4 inverse_projection_matrix = glm::inverse(camera.camera.proj_mat);
            glm::mat4 inverse_view_matrix = glm::inverse(camera.camera.view_mat);
            glm::mat4 projection_view_matrix = camera.camera.proj_mat * camera.camera.view_mat;
            glm::mat4 inverse_projection_view = inverse_projection_matrix * inverse_view_matrix;

            camera_info.projection_matrix =  camera.camera.proj_mat;
            camera_info.inverse_projection_matrix = inverse_projection_matrix;
            camera_info.view_matrix = camera.camera.view_mat;
            camera_info.inverse_view_matrix = inverse_view_matrix;
            camera_info.projection_view_matrix = projection_view_matrix;
            camera_info.inverse_projection_view_matrix = inverse_projection_view;
            camera_info.position = camera.position;

            std::array<glm::vec4, 6> planes = {};
            for (i32 i = 0; i < 4; ++i) { planes[0][i] = projection_view_matrix[i][3] + projection_view_matrix[i][0]; }
            for (i32 i = 0; i < 4; ++i) { planes[1][i] = projection_view_matrix[i][3] - projection_view_matrix[i][0]; }
            for (i32 i = 0; i < 4; ++i) { planes[2][i] = projection_view_matrix[i][3] + projection_view_matrix[i][1]; }
            for (i32 i = 0; i < 4; ++i) { planes[3][i] = projection_view_matrix[i][3] - projection_view_matrix[i][1]; }
            for (i32 i = 0; i < 4; ++i) { planes[4][i] = projection_view_matrix[i][3] + projection_view_matrix[i][2]; }
            for (i32 i = 0; i < 4; ++i) { planes[5][i] = projection_view_matrix[i][3] - projection_view_matrix[i][2]; }
            for (u32 i = 0; i < 6; ++i) {
                planes[i] /= glm::length(glm::vec3(planes[i]));
                planes[i].w = -planes[i].w;
                camera_info.frustum_planes[i] = planes[i];
            }

            return camera_info;
        };

        shader_globals.main_camera = set_camera_info(main_camera);
        shader_globals.observer_camera = set_camera_info(observer_camera);
    }
}
