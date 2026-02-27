#include "gpu_context.hpp"
#include <graphics/window.hpp>

#include "helper.hpp"

namespace foundation {
    ShaderDebugDrawContext::ShaderDebugDrawContext(GPUContext* _gpu_context) : gpu_context{_gpu_context} {
        PROFILE_SCOPE;
        usize size = sizeof(ShaderDebugBufferHead);
        size += sizeof(ShaderDebugAABBDraw) * aabb_draws.max_draws;
        size += sizeof(ShaderDebugCircleDraw) * circle_draws.max_draws;
        size += sizeof(ShaderDebugLineDraw) * line_draws.max_draws;

        this->buffer = gpu_context->device.create_buffer(daxa::BufferInfo {
            .size = size,
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "DebugDrawContext buffer"
        });
    }

    ShaderDebugDrawContext::~ShaderDebugDrawContext() {
        gpu_context->device.destroy_buffer(this->buffer);
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

        upload_draws(head.aabb_draw_indirect_info, aabb_draws, head.aabb_draws);
        upload_draws(head.circle_draw_indirect_info, circle_draws, head.circle_draws);
        upload_draws(head.line_draw_indirect_info, line_draws, head.line_draws);
    }

    GPUContext::GPUContext(NativeWindow* window)
        : instance{daxa::create_instance({})},
            device{[&](){
                daxa::ImplicitFeatureFlags required_implicit = 
                    daxa::ImplicitFeatureFlagBits::MESH_SHADER |
                    daxa::ImplicitFeatureFlagBits::BASIC_RAY_TRACING |
                    daxa::ImplicitFeatureFlagBits::RAY_TRACING_PIPELINE |
                    daxa::ImplicitFeatureFlagBits::RAY_TRACING_INVOCATION_REORDER |
                    daxa::ImplicitFeatureFlagBits::SHADER_ATOMIC_INT64 |
                    daxa::ImplicitFeatureFlagBits::IMAGE_ATOMIC64 |
                    daxa::ImplicitFeatureFlagBits::SWAPCHAIN;

                daxa::DeviceInfo2 device_info = {
                    .explicit_features = daxa::ExplicitFeatureFlagBits::BUFFER_DEVICE_ADDRESS_CAPTURE_REPLAY,
                    .max_allowed_images = 100000,
                    .max_allowed_buffers = 100000,
                };

                device_info = this->instance.choose_device(required_implicit, device_info);
                return this->instance.create_device_2(device_info); 
            }()},
            swapchain{window->create_swapchain(this->device)},
            pipeline_manager{daxa::PipelineManagerInfo2{
                .device = device,
                .root_paths = {
                        DAXA_SHADER_INCLUDE_DIR,
                        "./",
                        ""
                    },
                .write_out_preprocessed_code = "assets/cache/preproc",
                .write_out_spirv = "assets/cache/spv_raw",
                .spirv_cache_folder = "assets/cache/spv",
                .default_enable_debug_info = true,
                .name = "pipeline_manager",
            }},
            shader_globals{}, 
            shader_globals_buffer{make_task_buffer(this, {
                            .size = s_cast<u32>(sizeof(ShaderGlobals)),
                            .name = "globals",
                        })},
            shader_debug_draw_context{this} 
    {
        shader_globals.samplers = {
            .linear_clamp = this->device.create_sampler({
                .name = "linear clamp sampler",
            }),
            .linear_repeat = this->device.create_sampler({
                .address_mode_u = daxa::SamplerAddressMode::REPEAT,
                .address_mode_v = daxa::SamplerAddressMode::REPEAT,
                .address_mode_w = daxa::SamplerAddressMode::REPEAT,
                .name = "linear repeat sampler",
            }),
            .nearest_repeat = this->device.create_sampler({
                .magnification_filter = daxa::Filter::NEAREST,
                .minification_filter = daxa::Filter::NEAREST,
                .address_mode_u = daxa::SamplerAddressMode::REPEAT,
                .address_mode_v = daxa::SamplerAddressMode::REPEAT,
                .address_mode_w = daxa::SamplerAddressMode::REPEAT,
                .name = "linear repeat sampler",
            }),
            .nearest_clamp = this->device.create_sampler({
                .magnification_filter = daxa::Filter::NEAREST,
                .minification_filter = daxa::Filter::NEAREST,
                .mipmap_filter = daxa::Filter::NEAREST,
                .name = "nearest clamp sampler",
            }),
            .linear_repeat_ani = this->device.create_sampler({
                .address_mode_u = daxa::SamplerAddressMode::REPEAT,
                .address_mode_v = daxa::SamplerAddressMode::REPEAT,
                .address_mode_w = daxa::SamplerAddressMode::REPEAT,
                .mip_lod_bias = 0.0f,
                .enable_anisotropy = true,
                .max_anisotropy = 16.0f,
                .name = "linear repeat ani sampler",
            }),
            .nearest_repeat_ani = this->device.create_sampler({
                .magnification_filter = daxa::Filter::NEAREST,
                .minification_filter = daxa::Filter::NEAREST,
                .mipmap_filter = daxa::Filter::LINEAR,
                .address_mode_u = daxa::SamplerAddressMode::REPEAT,
                .address_mode_v = daxa::SamplerAddressMode::REPEAT,
                .address_mode_w = daxa::SamplerAddressMode::REPEAT,
                .mip_lod_bias = 0.0f,
                .enable_anisotropy = true,
                .max_anisotropy = 16.0f,
                .name = "nearest repeat ani sampler",
            }),
        };

        shader_globals.debug = device.buffer_device_address(shader_debug_draw_context.buffer).value();
    }

    GPUContext::~GPUContext() {
        {
            std::unique_lock<std::shared_mutex> lock{sampler_mutex};
            for (const auto& [info, sampler] : samplers) {
                device.destroy_sampler(sampler);
            }
            samplers.clear();
        }
        device.destroy_buffer(shader_globals_buffer.get_state().buffers[0]);
        device.destroy_sampler(std::bit_cast<daxa::SamplerId>(shader_globals.samplers.linear_clamp));
        device.destroy_sampler(std::bit_cast<daxa::SamplerId>(shader_globals.samplers.linear_repeat));
        device.destroy_sampler(std::bit_cast<daxa::SamplerId>(shader_globals.samplers.nearest_repeat));
        device.destroy_sampler(std::bit_cast<daxa::SamplerId>(shader_globals.samplers.nearest_clamp));
        device.destroy_sampler(std::bit_cast<daxa::SamplerId>(shader_globals.samplers.linear_repeat_ani));
        device.destroy_sampler(std::bit_cast<daxa::SamplerId>(shader_globals.samplers.nearest_repeat_ani));
        device.wait_idle();
        device.collect_garbage();
    }

    auto GPUContext::get_sampler(const daxa::SamplerInfo& info) -> daxa::SamplerId {
        PROFILE_SCOPE;

        {
            std::shared_lock<std::shared_mutex> read_lock{sampler_mutex};
            if (auto it = samplers.find(info); it != samplers.end()) {
                return it->second;
            }
        }

        std::unique_lock<std::shared_mutex> write_lock{sampler_mutex};
        if (auto it = samplers.find(info); it != samplers.end()) {
            return it->second;
        }

        auto sampler = device.create_sampler(info);
        samplers.emplace(info, sampler);
        return sampler;
    }
}
