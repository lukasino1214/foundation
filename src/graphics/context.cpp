#include "context.hpp"
#include <graphics/helper.hpp>

namespace foundation {
    DebugDrawContext::DebugDrawContext(Context* _context) : context{_context} {
        ZoneScoped;
        usize size = sizeof(ShaderDebugBufferHead);
        size += sizeof(ShaderDebugAABBDraw) * max_aabb_draws;

        this->buffer = context->create_buffer(daxa::BufferInfo {
            .size = size,
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "DebugDrawContext buffer"
        });

    }

    DebugDrawContext::~DebugDrawContext() {
        context->destroy_buffer(this->buffer);
    }

    void DebugDrawContext::update_debug_buffer(daxa::Device& device, daxa::CommandRecorder& recorder, daxa::TransferMemoryPool& allocator) {
        auto head = ShaderDebugBufferHead {
            .aabb_draw_indirect_info = {
                .vertex_count = aabb_vertices,
                .instance_count = s_cast<u32>(aabbs.size()),
                .first_vertex = 0,
                .first_instance = 0,
            },
            .aabb_draws = device.get_device_address(buffer).value() + sizeof(ShaderDebugBufferHead)
        };

        auto alloc = allocator.allocate_fill(head).value();
        recorder.copy_buffer_to_buffer({
            .src_buffer = allocator.buffer(),
            .dst_buffer = buffer,
            .src_offset = alloc.buffer_offset,
            .dst_offset = 0,
            .size = sizeof(ShaderDebugBufferHead),
        });

        if (!aabbs.empty())
        {
            u32 size = s_cast<u32>(aabbs.size() * sizeof(ShaderDebugAABBDraw));
            auto aabb_draws = allocator.allocate(size, 4).value();
            std::memcpy(aabb_draws.host_address, aabbs.data(), size);
            recorder.copy_buffer_to_buffer({
                .src_buffer = allocator.buffer(),
                .dst_buffer = buffer,
                .src_offset = aabb_draws.buffer_offset,
                .dst_offset = sizeof(ShaderDebugBufferHead),
                .size = size,
            });
            aabbs.clear();
        }
    }

    Context::Context(const AppWindow &window)
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
                .flags = daxa::DeviceFlagBits::BUFFER_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT | daxa::DeviceFlagBits::SHADER_ATOMIC64 | daxa::DeviceFlagBits::IMAGE_ATOMIC64,
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
            debug_draw_context{this} {
        shader_globals.debug = device.get_device_address(debug_draw_context.buffer).value();
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
        usize size = device.get_memory_requirements(info).size;
        image_memory_usage += size;
        return resource;
    }

    void Context::destroy_image(const daxa::ImageId& id) {
        usize size = device.get_memory_requirements(device.info_image(id).value()).size;
        image_memory_usage -= size;
        device.destroy_image(id);
    }

    void Context::destroy_image_deferred(daxa::CommandRecorder& cmd, const daxa::ImageId& id) {
        usize size = device.get_memory_requirements(device.info_image(id).value()).size;
        image_memory_usage -= size;
        cmd.destroy_image_deferred(id);
    }
    
    auto Context::create_buffer(const daxa::BufferInfo& info) -> daxa::BufferId {
        daxa::BufferId resource = device.create_buffer(info);
        usize size = device.get_memory_requirements(info).size;
        buffer_memory_usage += size;
        return resource;
    }

    void Context::destroy_buffer(const daxa::BufferId& id) {
        usize size = device.info_buffer(id).value().size;
        buffer_memory_usage -= size;
        device.destroy_buffer(id);
    }

    void Context::destroy_buffer_deferred(daxa::CommandRecorder& cmd, const daxa::BufferId& id) {
        usize size = device.info_buffer(id).value().size;
        buffer_memory_usage -= size;
        cmd.destroy_buffer_deferred(id);
    }

    void Context::update_shader_globals(ControlledCamera3D& camera, const glm::uvec2& size) {
        ZoneNamedN(update_shader_globals, "update shader globals", true);
        camera.camera.resize(s_cast<i32>(size.x), s_cast<i32>(size.y));

        glm::mat4 inverse_projection_matrix = glm::inverse(camera.camera.proj_mat);
        glm::mat4 inverse_view_matrix = glm::inverse(camera.camera.view_mat);
        glm::mat4 projection_view_matrix = camera.camera.proj_mat * camera.camera.view_mat;
        glm::mat4 inverse_projection_view = inverse_projection_matrix * inverse_view_matrix;

        shader_globals.camera_projection_matrix =  camera.camera.proj_mat;
        shader_globals.camera_inverse_projection_matrix = inverse_projection_matrix;
        shader_globals.camera_view_matrix = camera.camera.view_mat;
        shader_globals.camera_inverse_view_matrix = inverse_view_matrix;
        shader_globals.camera_projection_view_matrix = projection_view_matrix;
        shader_globals.camera_inverse_projection_view_matrix = inverse_projection_view;
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
            shader_globals.frustum_planes[i] = planes[i];
        }

        shader_globals.camera_position = camera.position;
    }
}
