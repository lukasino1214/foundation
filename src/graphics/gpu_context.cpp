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

        auto& postprocessing = shader_globals.postprocess_settings;
        postprocessing.min_luminance_log2 = {std::log2(1.0f / 4096.0f)};
        postprocessing.max_luminance_log2 = {std::log2(4096.0f)};
        postprocessing.luminance_adaption_tau = {1.0f};
        postprocessing.exposure_bias = {0.25f};
        postprocessing.calibration = {12.5f};
        postprocessing.sensor_sensitivity = {100.0f};
        postprocessing.luminance_log2_range = {postprocessing.max_luminance_log2 - postprocessing.min_luminance_log2};
        postprocessing.inv_luminance_log2_range = {1.0f / (postprocessing.max_luminance_log2 - postprocessing.min_luminance_log2)};
        postprocessing.auto_exposure_histogram_clip_lo = {0.6f};
        postprocessing.auto_exposure_histogram_clip_hi = {0.95f};

        auto& sky = shader_globals.sky_settings;

        sky.transmittance_dimensions = {256, 64};
        sky.transmittance_step_count = 400;
        sky.multiscattering_dimensions = {32, 32};
        sky.multiscattering_step_count = 20;
        sky.sky_dimensions = {256, 192};
        sky.sky_step_count = 50;

        sky.atmosphere_bottom = 6360.0f;
        sky.atmosphere_top = 6460.0f;

        sky.sun_brightness = 1.0f;
        sky.sun_direction = {0.0f, 1.0f, 0.0f};

        sky.mie_scattering = {0.003996f, 0.003996f, 0.003996f};
        sky.mie_extinction = {0.00444f, 0.00444f, 0.00444f};
        sky.mie_scale_height = 1.2f;
        sky.mie_phase_function_g = 0.8f;

        sky.rayleigh_scattering = {0.006604931f, 0.012344918f, 0.039412623f};
        sky.rayleigh_scale_height = 8.696f;

        sky.absorption_extinction = {0.00129072f, 0.00254036f, 0.0f};

        sky.mie_density[0] = {
            .layer_width = 11.0f,
            .exp_term = 1.19f,
            .exp_scale = -0.530f,
            .lin_term = 0.0f,
            .const_term = 0.266f
        };

        sky.mie_density[1] = {
            .layer_width = 74.7f,
            .exp_term = 0.730f,
            .exp_scale = -0.090f,
            .lin_term = 0.0f,
            .const_term = 0.0f
        };

        sky.rayleigh_density[0] = {
            .layer_width = 23.1f,
            .exp_term = 2.0f,
            .exp_scale = -0.325f,
            .lin_term = 0.0f,
            .const_term = 0.166f
        };

        sky.rayleigh_density[1] = {
            .layer_width = 100.0f,
            .exp_term = 0.0f,
            .exp_scale = -0.085f,
            .lin_term = 0.0f,
            .const_term = 0.021f
        };

        sky.absorption_density[0] = {
            .layer_width = 52.9f,
            .exp_term = 0.00182376393f,
            .exp_scale = 0.122f,
            .lin_term = 0.0f,
            .const_term = 0.0f
        };

        sky.absorption_density[1] = {
            .layer_width = 52.2f,
            .exp_term = 20.6245170027f,
            .exp_scale = -0.06666666666f,
            .lin_term = 0.0f,
            .const_term = 0.0f
        };

        daxa::DeviceAddress render_data_device_address = this->device.buffer_device_address(shader_globals_buffer.get_state().buffers[0]).value();
        const auto sky_settings_offset = offsetof(ShaderGlobals, sky_settings);

        const auto mie_density_offset = sky_settings_offset + offsetof(SkySettings, mie_density);
        shader_globals.sky_settings.mie_density_ptr = render_data_device_address + mie_density_offset;

        const auto rayleigh_density_offset = sky_settings_offset + offsetof(SkySettings, rayleigh_density);
        shader_globals.sky_settings.rayleigh_density_ptr = render_data_device_address + rayleigh_density_offset;

        const auto absoprtion_density_offset = sky_settings_offset + offsetof(SkySettings, absorption_density);
        shader_globals.sky_settings.absorption_density_ptr = render_data_device_address + absoprtion_density_offset;

        shader_globals.frame_index = 0;
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
