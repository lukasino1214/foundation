#include "context.hpp"
#include <graphics/helper.hpp>

namespace foundation {
    DebugDrawContext::DebugDrawContext(Context* _context) : context{_context} {
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
        std::string name = std::string{info.name.c_str().data()};
        auto find = images.resources.find(name);
        if(find != images.resources.end()) {
            throw std::runtime_error("image collision: " + name);
        } else {
            std::lock_guard<std::mutex> lock{*resource_mutex};
            daxa::ImageId resource = device.create_image(info);
            u32 size = s_cast<u32>(device.get_memory_requirements(info).size);
            images.resources[name] = ResourceHolder<daxa::ImageId>::Resource { resource, size };
            images.total_size += size;
            return resource;
        }
    }

    void Context::destroy_image(const daxa::ImageId& id) {
        std::lock_guard<std::mutex> lock{*resource_mutex};
        const daxa::ImageInfo& info = device.info_image(id).value();
        std::string name = std::string{info.name.c_str().data()};
        const auto& resource = images.resources.at(name);
        images.total_size -= resource.size;
        device.destroy_image(resource.resource);
        images.resources.erase(name);
    }

    void Context::destroy_image_deferred(daxa::CommandRecorder& cmd, const daxa::ImageId& id) {
        std::lock_guard<std::mutex> lock{*resource_mutex};
        const daxa::ImageInfo& info = device.info_image(id).value();
        std::string name = std::string{info.name.c_str().data()};
        const auto& resource = images.resources.at(name);
        images.total_size -= resource.size;
        cmd.destroy_image_deferred(resource.resource);
        images.resources.erase(name);
    }
    
    auto Context::create_buffer(const daxa::BufferInfo& info) -> daxa::BufferId {
        std::string name = std::string{info.name.c_str().data()};
        auto find = buffers.resources.find(name);
        if(find != buffers.resources.end()) {
            throw std::runtime_error("buffer collision: " + name);
        } else {
            std::lock_guard<std::mutex> lock{*resource_mutex};
            daxa::BufferId resource = device.create_buffer(info);
            u32 size = s_cast<u32>(device.get_memory_requirements(info).size);
            buffers.resources[name] = ResourceHolder<daxa::BufferId>::Resource { resource, size };
            buffers.total_size += size;
            return resource;
        }
    }

    void Context::destroy_buffer(const daxa::BufferId& id) {
        std::lock_guard<std::mutex> lock{*resource_mutex};
        const daxa::BufferInfo& info = device.info_buffer(id).value();
        std::string name = std::string{info.name.c_str().data()};
        const auto& resource = buffers.resources.at(name);
        buffers.total_size -= resource.size;
        device.destroy_buffer(resource.resource);
        buffers.resources.erase(name);
    }

    void Context::destroy_buffer_deferred(daxa::CommandRecorder& cmd, const daxa::BufferId& id) {
        std::lock_guard<std::mutex> lock{*resource_mutex};
        const daxa::BufferInfo& info = device.info_buffer(id).value();
        std::string name = std::string{info.name.c_str().data()};
        const auto& resource = buffers.resources.at(name);
        buffers.total_size -= resource.size;
        cmd.destroy_buffer_deferred(resource.resource);
        buffers.resources.erase(name);
    }

    void Context::update_shader_globals(ControlledCamera3D& camera, const glm::uvec2& size) {
        camera.camera.resize(static_cast<i32>(size.x), static_cast<i32>(size.y));

        glm::mat4 inverse_projection_matrix = glm::inverse(camera.camera.proj_mat);
        glm::mat4 inverse_view_matrix = glm::inverse(camera.camera.view_mat);
        glm::mat4 projection_view_matrix = camera.camera.proj_mat * camera.camera.view_mat;
        glm::mat4 inverse_projection_view = inverse_projection_matrix * inverse_view_matrix;

        shader_globals.camera_projection_matrix = *reinterpret_cast<const daxa_f32mat4x4*>(&camera.camera.proj_mat);
        shader_globals.camera_inverse_projection_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&inverse_projection_matrix);
        shader_globals.camera_view_matrix = *reinterpret_cast<const daxa_f32mat4x4*>(&camera.camera.view_mat);
        shader_globals.camera_inverse_view_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&inverse_view_matrix);
        shader_globals.camera_projection_view_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&projection_view_matrix);
        shader_globals.camera_inverse_projection_view_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&inverse_projection_view);
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
            shader_globals.frustum_planes[i] = *reinterpret_cast<daxa_f32vec4*>(&planes[i]);
        }

        shader_globals.camera_position = *reinterpret_cast<const daxa_f32vec3*>(&camera.position);
    }
}
