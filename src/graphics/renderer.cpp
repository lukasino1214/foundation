#include <graphics/renderer.hpp>
#include <imgui_impl_glfw.h>

#include <graphics/helper.hpp>
#include "common/tasks/clear_image.inl"
#include "common/tasks/debug.inl"
#include "common/tasks/debug_image.inl"
#include <graphics/virtual_geometry/tasks/cull_meshlets.inl>
#include <graphics/virtual_geometry/tasks/generate_hiz.inl>
#include <graphics/virtual_geometry/tasks/resolve_visibility_buffer.inl>
#include <graphics/virtual_geometry/tasks/software_rasterization.inl>
#include <graphics/virtual_geometry/tasks/software_rasterization_only_depth.inl>
#include <graphics/virtual_geometry/tasks/cull_meshes.inl>
#include <graphics/virtual_geometry/tasks/draw_meshlets.inl>
#include <graphics/virtual_geometry/tasks/draw_meshlets_only_depth.inl>
#include <graphics/virtual_geometry/tasks/draw_meshlets_masked.inl>
#include <graphics/virtual_geometry/tasks/draw_meshlets_transparent.inl>
#include <graphics/virtual_geometry/tasks/draw_meshlets_only_depth_masked.inl>
#include <graphics/virtual_geometry/tasks/combine_depth.inl>
#include <graphics/virtual_geometry/tasks/extract_depth.inl>
#include <graphics/virtual_geometry/tasks/resolve_wboit.inl>
#include <graphics/post_processing/tasks/generate_gbuffer.inl>
#include <graphics/post_processing/tasks/generate_ssao.inl>
#include <graphics/post_processing/tasks/calculate_frustums.inl>
#include <graphics/post_processing/tasks/cull_lights.inl>
#include <ImGuizmo.h>

namespace foundation {
    struct MiscellaneousTasks {
        static inline constexpr std::string_view UPDATE_SCENE = "update scene";
        static inline constexpr std::string_view UPDATE_GLOBALS = "update globals";
        static inline constexpr std::string_view READBACK_COPY = "readback copy";
        static inline constexpr std::string_view READBACK_RAM = "readback ram";
        static inline constexpr std::string_view IMGUI_DRAW = "imgui draw";
    };

    struct VirtualGeometryTasks {
        static inline constexpr std::string_view CLEAR_DEPTH_IMAGE_U32 = "clear depth image u32";
        static inline constexpr std::string_view CLEAR_VISIBILITY_IMAGE = "clear visibility image";
    };

    struct PostProcessingTasks {
        static inline constexpr std::string_view CLEAR_GENERATE_GBUFFER_IMAGES = "clear generate gbuffer images";
    };

    Renderer::Renderer(NativeWIndow* _window, Context* _context, Scene* _scene, AssetManager* _asset_manager) 
        : window{_window}, context{_context}, scene{_scene}, asset_manager{_asset_manager} {
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(window->glfw_handle, true);
        imgui_renderer =  daxa::ImGuiRenderer({
            .device = context->device,
            .format = context->swapchain.get_format(),
            .context = ImGui::GetCurrentContext()
        });
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;

        swapchain_image = daxa::TaskImage{{.swapchain_image = true, .name = "swapchain image"}};
        render_image = daxa::TaskImage{{ .name = "render image" }};
        visibility_image = daxa::TaskImage{{ .name = "visibility image" }};
        depth_image_d32 = daxa::TaskImage{{ .name = "depth image d32" }};
        depth_image_u32 = daxa::TaskImage{{ .name = "depth image u32" }};
        depth_image_f32 = daxa::TaskImage{{ .name = "depth image f32" }};
        
        overdraw_image = daxa::TaskImage{{ .name = "overdraw image" }};

        wboit_accumulation_image = daxa::TaskImage{{ .name = "wboit accumulation image" }};
        wboit_reveal_image = daxa::TaskImage{{ .name = "wboit reveal image" }};

        ssao_image = daxa::TaskImage{{ .name = "ssao image" }};
        // ssao_blur_image = daxa::TaskImage{{ .name = "ssao blur image" }};
        // ssao_upsample_image = daxa::TaskImage{{ .name = "ssao upsample image" }};

        normal_image = daxa::TaskImage{{ .name = "normal image" }};
        normal_half_res_image = daxa::TaskImage{{ .name = "normal half res image" }};
        normal_detail_image = daxa::TaskImage{{ .name = "normal detail image" }};
        normal_detail_half_res_image = daxa::TaskImage{{ .name = "normal detail half res image" }};
        depth_half_res_image = daxa::TaskImage{{ .name = "depth half res image" }};

        images = {
            render_image,
            depth_image_d32,
            depth_image_u32,
            depth_image_f32,
            visibility_image,
            overdraw_image,
            wboit_accumulation_image,
            wboit_reveal_image,
            ssao_image,
            // ssao_blur_image,
            // ssao_upsample_image,
            normal_image,
            normal_half_res_image,
            normal_detail_image,
            normal_detail_half_res_image,
            depth_half_res_image,
        };
        frame_buffer_images = {
            {
                {
                    .format = daxa::Format::R8G8B8A8_UNORM,
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = render_image.info().name,
                },
                render_image,
            },
            {
                {
                    .format = daxa::Format::D32_SFLOAT,
                    .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = depth_image_d32.info().name,
                },
                depth_image_d32,
            },
            {
                {
                    .format = daxa::Format::R32_UINT,
                    .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = depth_image_u32.info().name,
                },
                depth_image_u32,
            },
            {
                {
                    .format = daxa::Format::R32_SFLOAT,
                    .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = depth_image_f32.info().name,
                },
                depth_image_f32,
            },
            {
                {
                    .format = daxa::Format::R64_UINT,
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = visibility_image.info().name,
                },
                visibility_image,
            },
            {
                {
                    .format = daxa::Format::R32_UINT,
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = overdraw_image.info().name,
                },
                overdraw_image,
            },
            {
                {
                    .format = daxa::Format::R16G16B16A16_SFLOAT,
                    .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = wboit_accumulation_image.info().name,
                },
                wboit_accumulation_image,
            },
            {
                {
                    .format = daxa::Format::R32_SFLOAT,
                    .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = wboit_reveal_image.info().name,
                },
                wboit_reveal_image,
            },
            {
                {
                    .format = daxa::Format::R8_UNORM,
                    .usage = daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = ssao_image.info().name,
                },
                ssao_image,
            },
            // {
            //     {
            //         .format = daxa::Format::R8_UNORM,
            //         .usage = daxa::ImageUsageFlagBits::SHADER_STORAGE,
            //         .name = ssao_blur_image.info().name,
            //     },
            //     ssao_blur_image,
            // },
            // {
            //     {
            //         .format = daxa::Format::R8_UNORM,
            //         .usage = daxa::ImageUsageFlagBits::SHADER_STORAGE,
            //         .name = ssao_upsample_image.info().name,
            //     },
            //     ssao_upsample_image,
            // },
            {
                {
                    .format = daxa::Format::R32_UINT,
                    .usage = daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = normal_image.info().name,
                },
                normal_image,
            },
            {
                {
                    .format = daxa::Format::R32_UINT,
                    .usage = daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = normal_half_res_image.info().name,
                },
                normal_half_res_image,
            },
            {
                {
                    .format = daxa::Format::R32_UINT,
                    .usage = daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = normal_detail_image.info().name,
                },
                normal_detail_image,
            },
            {
                {
                    .format = daxa::Format::R32_UINT,
                    .usage = daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = normal_detail_half_res_image.info().name,
                },
                normal_detail_half_res_image,
            },
            {
                {
                    .format = daxa::Format::R32_SFLOAT,
                    .usage = daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = depth_half_res_image.info().name,
                },
                depth_half_res_image,
            },
        };

        sun_light_buffer = make_task_buffer(context, daxa::BufferInfo {
            .size = sizeof(SunLight),
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "sun light buffer"
        });

        point_light_buffer = make_task_buffer(context, daxa::BufferInfo {
            .size = sizeof(PointLightsData) + (sizeof(PointLight) * MAX_POINT_LIGHTS),
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "point light buffer"
        });

        spot_light_buffer = make_task_buffer(context, daxa::BufferInfo {
            .size = sizeof(SpotLightsData) + (sizeof(SpotLight) * MAX_SPOT_LIGHTS),
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "spot light buffer"
        });

        tile_frustums_buffer = make_task_buffer(context, daxa::BufferInfo {
            .size = sizeof(TileFrustum),
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "tile frustums buffer"
        });

        tile_data_buffer = make_task_buffer(context, daxa::BufferInfo {
            .size = sizeof(TileData),
            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
            .name = "tile data buffer"
        });

        buffers = {
            sun_light_buffer,
            point_light_buffer,
            spot_light_buffer,
            tile_frustums_buffer,
            tile_data_buffer,
        };

        // TODO: remove this
        auto cmd_recorder = context->device.create_command_recorder({});

        {
            daxa::BufferId stagging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = sizeof(SunLight),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "sun stagging_buffer"
            });
            context->destroy_buffer_deferred(cmd_recorder, stagging_buffer);

            SunLight light = {
                .direction = { -1.0f, -1.0f, -1.0f },
                .color = { 1.0f, 1.0f, 1.0f },
                .intensity = 1.0f
            };

            std::memcpy(context->device.buffer_host_address(stagging_buffer).value(), &light, sizeof(SunLight));

            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = stagging_buffer,
                .dst_buffer = sun_light_buffer.get_state().buffers[0],
                .size = sizeof(SunLight)
            });
        }

        {
            daxa::BufferId stagging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = sizeof(PointLightsData) + (sizeof(PointLight) * MAX_POINT_LIGHTS),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "point stagging_buffer"
            });
            context->destroy_buffer_deferred(cmd_recorder, stagging_buffer);

            PointLightsData light = {
                .count = MAX_POINT_LIGHTS,
                .point_lights = context->device.buffer_device_address(point_light_buffer.get_state().buffers[0]).value() + sizeof(PointLightsData) 
            };

            std::memcpy(context->device.buffer_host_address(stagging_buffer).value(), &light, sizeof(PointLightsData));

            std::vector<PointLight> point_lights = {};
            point_lights.reserve(MAX_POINT_LIGHTS);

            for(u32 x = 0; x < 16; x++) {
                for(u32 y = 0; y < 16; y++) {
                    point_lights.push_back(PointLight {
                        .position = { x, 1, y },
                        .color = { 1.0f, 1.0f, 1.0f },
                        .intensity = 1.0f,
                        .range = 1.0f,
                    });
                }
            }

            std::memcpy(context->device.buffer_host_address(stagging_buffer).value() + sizeof(PointLightsData), point_lights.data(), sizeof(PointLight) * MAX_POINT_LIGHTS);
            
            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = stagging_buffer,
                .dst_buffer = point_light_buffer.get_state().buffers[0],
                .size = sizeof(PointLightsData) + (sizeof(PointLight) * MAX_POINT_LIGHTS)
            });
        }

        {
            daxa::BufferId stagging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = sizeof(SpotLightsData) + (sizeof(SpotLight) * MAX_POINT_LIGHTS),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "spot stagging_buffer"
            });
            context->destroy_buffer_deferred(cmd_recorder, stagging_buffer);

            SpotLightsData light = {
                .count = MAX_POINT_LIGHTS,
                .spot_lights = context->device.buffer_device_address(spot_light_buffer.get_state().buffers[0]).value() + sizeof(SpotLightsData) 
            };

            std::memcpy(context->device.buffer_host_address(stagging_buffer).value(), &light, sizeof(SpotLightsData));

            std::vector<SpotLight> spot_lights = {};
            spot_lights.reserve(MAX_POINT_LIGHTS);

            for(u32 x = 0; x < 16; x++) {
                for(u32 y = 0; y < 16; y++) {
                    spot_lights.push_back(SpotLight {
                        .position = { x, 1, y },
                        .direction = { 0, -1, 0 },
                        .color = { 1.0f, 0.0f, 0.0f },
                        .intensity = 1.0f,
                        .range = 1.0f,
                        .inner_cone_angle = 0.0f,
                        .outer_cone_angle = std::numbers::pi_v<f32> / 4.0f
                    });
                }
            }

            std::memcpy(context->device.buffer_host_address(stagging_buffer).value() + sizeof(SpotLightsData), spot_lights.data(), sizeof(SpotLight) * MAX_POINT_LIGHTS);
            
            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = stagging_buffer,
                .dst_buffer = spot_light_buffer.get_state().buffers[0],
                .size = sizeof(SpotLightsData) + (sizeof(SpotLight) * MAX_POINT_LIGHTS)
            });
        }

        context->device.submit_commands(daxa::CommandSubmitInfo {
            .command_lists = std::to_array({ cmd_recorder.complete_current_commands() })
        });
        
        context->device.wait_idle();
        context->device.wait_idle();

        context->gpu_metrics[ClearImageTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DebugImageTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());

        context->gpu_metrics[DebugEntityOOBDrawTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DebugAABBDrawTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DebugCircleDrawTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DebugLineDrawTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());

        context->gpu_metrics[MiscellaneousTasks::UPDATE_SCENE] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[MiscellaneousTasks::UPDATE_GLOBALS] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[MiscellaneousTasks::READBACK_COPY] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[MiscellaneousTasks::READBACK_RAM] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[MiscellaneousTasks::IMGUI_DRAW] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        
        context->gpu_metrics[VirtualGeometryTasks::CLEAR_DEPTH_IMAGE_U32] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[VirtualGeometryTasks::CLEAR_VISIBILITY_IMAGE] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        
        // early pass
        context->gpu_metrics[DrawMeshletsOnlyDepthWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsOnlyDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsOnlyDepthMaskedWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsOnlyDepthMaskedTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationOnlyDepthWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationOnlyDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CombineDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        
        // culling
        context->gpu_metrics[GenerateHizTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshesWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshesTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsOpaqueWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsOpaqueTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsMaskedWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsMaskedTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsTransparentWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullMeshletsTransparentTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        
        // late pass
        context->gpu_metrics[DrawMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsMaskedWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsMaskedTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsTransparentWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsTransparentTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[SoftwareRasterizationTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[ExtractDepthTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());

        // resolve
        context->gpu_metrics[ResolveVisibilityBufferTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[ResolveWBOITTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());

        context->gpu_metrics[PostProcessingTasks::CLEAR_GENERATE_GBUFFER_IMAGES] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[GenerateGBufferTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[GenerateSSAOTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CalculateFrustumsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[CullLightsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());

        rebuild_task_graph();

        context->shader_globals.samplers = {
            .linear_clamp = context->get_sampler(daxa::SamplerInfo {
                .name = "linear clamp sampler",
            }),
            .linear_repeat = context->get_sampler(daxa::SamplerInfo {
                .address_mode_u = daxa::SamplerAddressMode::REPEAT,
                .address_mode_v = daxa::SamplerAddressMode::REPEAT,
                .address_mode_w = daxa::SamplerAddressMode::REPEAT,
                .name = "linear repeat sampler",
            }),
            .nearest_repeat = context->get_sampler(daxa::SamplerInfo {
                .magnification_filter = daxa::Filter::NEAREST,
                .minification_filter = daxa::Filter::NEAREST,
                .address_mode_u = daxa::SamplerAddressMode::REPEAT,
                .address_mode_v = daxa::SamplerAddressMode::REPEAT,
                .address_mode_w = daxa::SamplerAddressMode::REPEAT,
                .name = "linear repeat sampler",
            }),
            .nearest_clamp = context->get_sampler(daxa::SamplerInfo {
                .magnification_filter = daxa::Filter::NEAREST,
                .minification_filter = daxa::Filter::NEAREST,
                .mipmap_filter = daxa::Filter::NEAREST,
                .name = "nearest clamp sampler",
            }),
            .linear_repeat_anisotropy = context->get_sampler(daxa::SamplerInfo {
                .address_mode_u = daxa::SamplerAddressMode::REPEAT,
                .address_mode_v = daxa::SamplerAddressMode::REPEAT,
                .address_mode_w = daxa::SamplerAddressMode::REPEAT,
                .mip_lod_bias = 0.0f,
                .enable_anisotropy = true,
                .max_anisotropy = 16.0f,
                .name = "linear repeat ani sampler",
            }),
            .nearest_repeat_anisotropy = context->get_sampler(daxa::SamplerInfo {
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

        std::vector<PerfomanceCategory> virtual_geometry_performance_metrics = {
            PerfomanceCategory {
                .name = "early pass",
                .counters = {
                    {VirtualGeometryTasks::CLEAR_DEPTH_IMAGE_U32, "clear depth image u32"},
                    {DrawMeshletsOnlyDepthWriteCommandTask::name(), "hw draw meshlets write"},
                    {DrawMeshletsOnlyDepthTask::name(), "hw draw meshlets"},
                    {DrawMeshletsOnlyDepthMaskedWriteCommandTask::name(), "hw draw meshlets masked write"},
                    {DrawMeshletsOnlyDepthMaskedTask::name(), "hw draw meshlets masked"},
                    {SoftwareRasterizationOnlyDepthWriteCommandTask::name(), "sw draw meshlets write"},
                    {SoftwareRasterizationOnlyDepthTask::name(), "sw draw meshlets"},
                    {CombineDepthTask::name(), "combine depth"},
                }
            },
            PerfomanceCategory {
                .name = "culling",
                .counters = {
                    {GenerateHizTask::name(), "generate hiz"},
                    {CullMeshesWriteCommandTask::name(), "cull meshes write"},
                    {CullMeshesTask::name(), "cull meshes"},
                    {CullMeshletsOpaqueWriteCommandTask::name(), "cull meshlets opaque write"},
                    {CullMeshletsOpaqueTask::name(), "cull meshlets opaque"},
                    {CullMeshletsMaskedWriteCommandTask::name(), "cull meshlets masked write"},
                    {CullMeshletsMaskedTask::name(), "cull meshlets masked"},
                    {CullMeshletsTransparentWriteCommandTask::name(), "cull meshlets write transparent"},
                    {CullMeshletsTransparentTask::name(), "cull meshlets transparent"},
                }
            },
            PerfomanceCategory {
                .name = "late pass",
                .counters = {
                    {VirtualGeometryTasks::CLEAR_VISIBILITY_IMAGE, "clear visibility image"},
                    {DrawMeshletsWriteCommandTask::name(), "hw draw meshlets write"},
                    {DrawMeshletsTask::name(), "hw draw meshlets"},
                    {DrawMeshletsMaskedWriteCommandTask::name(), "hw draw meshlets masked write"},
                    {DrawMeshletsMaskedTask::name(), "hw draw meshlets masked"},
                    {DrawMeshletsTransparentWriteCommandTask::name(), "hw draw meshlets transparent write"},
                    {DrawMeshletsTransparentTask::name(), "hw draw transparent masked"},
                    {SoftwareRasterizationWriteCommandTask::name(), "sw draw meshlets write"},
                    {SoftwareRasterizationTask::name(), "sw draw meshlets"},
                    {ExtractDepthTask::name(), "extract depth"},
                }
            },
            PerfomanceCategory {
                .name = "resolve",
                .counters = {
                    {ResolveVisibilityBufferTask::name(), "resolve visibility buffer"},
                    {ResolveWBOITTask::name(), "resolve wboit"}
                }
            },
            PerfomanceCategory {
                .name = "post processing",
                .counters = {
                    {PostProcessingTasks::CLEAR_GENERATE_GBUFFER_IMAGES, "clear gbuffer images"},
                    {GenerateGBufferTask::name(), "generate gbuffer"},
                    {GenerateSSAOTask::name(), "generate ssao"},
                    {CalculateFrustumsTask::name(), "calculate frustums"},
                    {CullLightsTask::name(), "cull lights"},
                }
            },
        };

        performace_metrics.insert(performace_metrics.end(), virtual_geometry_performance_metrics.begin(), virtual_geometry_performance_metrics.end());
        performace_metrics.push_back(PerfomanceCategory {
            .name = "miscellaneous",
            .counters = {
                {MiscellaneousTasks::UPDATE_SCENE, "update scene"},
                {MiscellaneousTasks::UPDATE_GLOBALS, "update globals"},
                {MiscellaneousTasks::READBACK_COPY, "readback copy"},
                {MiscellaneousTasks::READBACK_RAM, "readback ram"},
                {MiscellaneousTasks::IMGUI_DRAW, "imgui draw"},
            }
        });
    }

    Renderer::~Renderer() {
        for(auto& task_image : images) {
            for(auto image : task_image.get_state().images) {
                context->destroy_image(image);
            }
        }

        for (auto& task_buffer : buffers) {
            for (auto buffer : task_buffer.get_state().buffers) {
                context->destroy_buffer(buffer);
            }
        }

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        this->context->device.wait_idle();
        this->context->device.collect_garbage();
    }

    void Renderer::render() {
        PROFILE_SCOPE;
        {
            PROFILE_SCOPE_NAMED(pipelines);
            auto reloaded_result = context->pipeline_manager.reload_all();
            if (auto* reload_err = daxa::get_if<daxa::PipelineReloadError>(&reloaded_result)) {
                std::cout << "Failed to reload " << reload_err->message << '\n';
            }
            if (daxa::get_if<daxa::PipelineReloadSuccess>(&reloaded_result) != nullptr) {
                std::cout << "Successfully reloaded!\n";
            }
        }

        auto image = context->swapchain.acquire_next_image();
        if(image.is_empty()) { return; }
        swapchain_image.set_images({.images = std::span{&image, 1}});
 
        {
            PROFILE_SCOPE_NAMED(rendergraph_execute);
            render_task_graph.execute({});
        }
        {
            PROFILE_SCOPE_NAMED(garbage);
            context->device.collect_garbage();
        }
    }

    void Renderer::window_resized() const {
        context->swapchain.resize();
    }

    void Renderer::recreate_framebuffer(const glm::uvec2& size) {
        LOG_INFO("resizing framebuffers from [{}, {}] to [{}, {}]", context->shader_globals.render_target_size.x, context->shader_globals.render_target_size.y, size.x, size.y);

        context->destroy_buffer(tile_frustums_buffer.get_state().buffers[0]);
        tile_frustums_buffer.set_buffers({ .buffers = std::array{
            context->create_buffer(daxa::BufferInfo {
                .size = sizeof(TileFrustum) * std::max(round_up_div(size.x, PIXELS_PER_FRUSTUM), 1u) * std::max(round_up_div(size.y, PIXELS_PER_FRUSTUM), 1u),
                .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                .name = "tile frustums buffer"
            })
        }});

        context->destroy_buffer(tile_data_buffer.get_state().buffers[0]);
        tile_data_buffer.set_buffers({ .buffers = std::array{
            context->create_buffer(daxa::BufferInfo {
                .size = sizeof(TileData) * std::max(round_up_div(size.x, PIXELS_PER_FRUSTUM), 1u) * std::max(round_up_div(size.y, PIXELS_PER_FRUSTUM), 1u),
                .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                .name = "tile data buffer"
            })
        }});

        for (auto &[info, timg] : frame_buffer_images) {
            for(auto image : timg.get_state().images) {
                context->destroy_image(image);
            }

            info.size = { std::max(size.x, 1u), std::max(size.y, 1u), 1 };

            if(std::string{info.name.c_str().data()} == "ssao image") {
                info.size = { round_up_div(size.x, 2), round_up_div(size.y, 2), 1 };
            }

            if(std::string{info.name.c_str().data()} == "ssao blur image") {
                info.size = { round_up_div(size.x, 2), round_up_div(size.y, 2), 1 };
            }

            if(std::string{info.name.c_str().data()} == "normal half res image") {
                info.size = { round_up_div(size.x, 2), round_up_div(size.y, 2), 1 };
            }

            if(std::string{info.name.c_str().data()} == "normal detail half res image") {
                info.size = { round_up_div(size.x, 2), round_up_div(size.y, 2), 1 };
            }

            if(std::string{info.name.c_str().data()} == "depth half res image") {
                info.size = { round_up_div(size.x, 2), round_up_div(size.y, 2), 1 };
            }

            timg.set_images({.images = std::array{context->create_image(info)}});
        }

        context->shader_globals.render_target_size = { size.x, size.y };
        context->shader_globals.render_target_size_inv = {
            1.0f / s_cast<f32>(context->shader_globals.render_target_size.x),
            1.0f / s_cast<f32>(context->shader_globals.render_target_size.y),
        };
        context->shader_globals.next_lower_po2_render_target_size.x = find_next_lower_po2(context->shader_globals.render_target_size.x);
        context->shader_globals.next_lower_po2_render_target_size.y = find_next_lower_po2(context->shader_globals.render_target_size.y);
        context->shader_globals.next_lower_po2_render_target_size_inv = {
            1.0f / s_cast<f32>(context->shader_globals.next_lower_po2_render_target_size.x),
            1.0f / s_cast<f32>(context->shader_globals.next_lower_po2_render_target_size.y),
        };

        rebuild_task_graph();
    }

    void Renderer::compile_pipelines() {
        std::vector<std::tuple<std::string_view, daxa::RasterPipelineCompileInfo2>> rasters = {
            {DebugEntityOOBDrawTask::name(), DebugEntityOOBDrawTask::pipeline_config_info()},
            {DebugAABBDrawTask::name(), DebugAABBDrawTask::pipeline_config_info()},
            {DebugCircleDrawTask::name(), DebugCircleDrawTask::pipeline_config_info()},
            {DebugLineDrawTask::name(), DebugLineDrawTask::pipeline_config_info()},

            {DrawMeshletsTask::name(), DrawMeshletsTask::pipeline_config_info()},
            {DrawMeshletsOnlyDepthTask::name(), DrawMeshletsOnlyDepthTask::pipeline_config_info()},
            {DrawMeshletsMaskedTask::name(), DrawMeshletsMaskedTask::pipeline_config_info()},
            {DrawMeshletsTransparentTask::name(), DrawMeshletsTransparentTask::pipeline_config_info()},
            {DrawMeshletsOnlyDepthMaskedTask::name(), DrawMeshletsOnlyDepthMaskedTask::pipeline_config_info()},
            {ExtractDepthTask::name(), ExtractDepthTask::pipeline_config_info()},
            {ResolveWBOITTask::name(), ResolveWBOITTask::pipeline_config_info()},
        };

        for (auto [name, info] : rasters) {
            auto compilation_result = this->context->pipeline_manager.add_raster_pipeline2(info);
            if(compilation_result.is_ok()) {
                std::println("SUCCESSFULLY compiled pipeline {}", name);
            } else {
                std::println("FAILED to compile pipeline {} with message \n {}", name, compilation_result.message());
            }

            this->context->raster_pipelines[name] = compilation_result.value();
        }

        std::vector<std::tuple<std::string_view, daxa::ComputePipelineCompileInfo2>> computes = {
            {ClearImageTask::name(), ClearImageTask::pipeline_config_info()},
            {DebugImageTask::name(), DebugImageTask::pipeline_config_info()},

            {CullMeshletsOpaqueWriteCommandTask::name(), CullMeshletsOpaqueWriteCommandTask::pipeline_config_info()},
            {CullMeshletsOpaqueTask::name(), CullMeshletsOpaqueTask::pipeline_config_info()},
            {CullMeshletsMaskedWriteCommandTask::name(), CullMeshletsMaskedWriteCommandTask::pipeline_config_info()},
            {CullMeshletsMaskedTask::name(), CullMeshletsMaskedTask::pipeline_config_info()},
            {CullMeshletsTransparentWriteCommandTask::name(), CullMeshletsTransparentWriteCommandTask::pipeline_config_info()},
            {CullMeshletsTransparentTask::name(), CullMeshletsTransparentTask::pipeline_config_info()},
            {GenerateHizTask::name(), GenerateHizTask::pipeline_config_info()},
            {ResolveVisibilityBufferTask::name(), ResolveVisibilityBufferTask::pipeline_config_info()},
            {CullMeshesWriteCommandTask::name(), CullMeshesWriteCommandTask::pipeline_config_info()},
            {CullMeshesTask::name(), CullMeshesTask::pipeline_config_info()},
            {DrawMeshletsWriteCommandTask::name(), DrawMeshletsWriteCommandTask::pipeline_config_info()},
            {DrawMeshletsOnlyDepthWriteCommandTask::name(), DrawMeshletsOnlyDepthWriteCommandTask::pipeline_config_info()},
            {DrawMeshletsMaskedWriteCommandTask::name(), DrawMeshletsMaskedWriteCommandTask::pipeline_config_info()},
            {DrawMeshletsTransparentWriteCommandTask::name(), DrawMeshletsTransparentWriteCommandTask::pipeline_config_info()},
            {DrawMeshletsOnlyDepthMaskedWriteCommandTask::name(), DrawMeshletsOnlyDepthMaskedWriteCommandTask::pipeline_config_info()},
            {CombineDepthTask::name(), CombineDepthTask::pipeline_config_info()},
            {SoftwareRasterizationWriteCommandTask::name(), SoftwareRasterizationWriteCommandTask::pipeline_config_info()},
            {SoftwareRasterizationOnlyDepthWriteCommandTask::name(), SoftwareRasterizationOnlyDepthWriteCommandTask::pipeline_config_info()},
            {SoftwareRasterizationTask::name(), SoftwareRasterizationTask::pipeline_config_info()},
            {SoftwareRasterizationOnlyDepthTask::name(), SoftwareRasterizationOnlyDepthTask::pipeline_config_info()},

            {GenerateGBufferTask::name(), GenerateGBufferTask::pipeline_config_info()},
            {GenerateSSAOTask::name(), GenerateSSAOTask::pipeline_config_info()},
            {CalculateFrustumsTask::name(), CalculateFrustumsTask::pipeline_config_info()},
            {CullLightsTask::name(), CullLightsTask::pipeline_config_info()},
        };

        for (auto [name, info] : computes) {
            auto compilation_result = this->context->pipeline_manager.add_compute_pipeline2(info);
            if(compilation_result.is_ok()) {
                std::println("SUCCESSFULLY compiled pipeline {}", name);
            } else {
                std::println("FAILED to compile pipeline {} with message \n {}", name, compilation_result.message());
            }
            
            this->context->compute_pipelines[name] = compilation_result.value();
        }
    }

    void Renderer::rebuild_task_graph() {
        render_task_graph = daxa::TaskGraph({
            .device = context->device,
            .swapchain = context->swapchain,
            .staging_memory_pool_size = 1u << 24, // cope
            .name = "render task graph",
        });

        auto& shader_globals_buffer = context->shader_globals_buffer;

        render_task_graph.use_persistent_image(swapchain_image);
        render_task_graph.use_persistent_image(render_image);
        render_task_graph.use_persistent_image(depth_image_d32);
        render_task_graph.use_persistent_image(depth_image_u32);
        render_task_graph.use_persistent_image(depth_image_f32);
        render_task_graph.use_persistent_image(visibility_image);
        render_task_graph.use_persistent_image(overdraw_image);
        render_task_graph.use_persistent_image(wboit_accumulation_image);
        render_task_graph.use_persistent_image(wboit_reveal_image);

        render_task_graph.use_persistent_image(normal_image);
        render_task_graph.use_persistent_image(normal_half_res_image);
        render_task_graph.use_persistent_image(normal_detail_image);
        render_task_graph.use_persistent_image(normal_detail_half_res_image);
        render_task_graph.use_persistent_image(depth_half_res_image);

        render_task_graph.use_persistent_image(ssao_image);
        // render_task_graph.use_persistent_image(ssao_blur_image);
        // render_task_graph.use_persistent_image(ssao_upsample_image);

        render_task_graph.use_persistent_buffer(tile_frustums_buffer);
        render_task_graph.use_persistent_buffer(tile_data_buffer);

        render_task_graph.use_persistent_buffer(shader_globals_buffer);
        render_task_graph.use_persistent_buffer(scene->gpu_transforms_pool.task_buffer);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_materials);
        render_task_graph.use_persistent_buffer(scene->gpu_scene_data);
        render_task_graph.use_persistent_buffer(scene->gpu_entities_data_pool.task_buffer);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_mesh_groups);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_mesh_indices);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_meshes);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_meshlets_instance_data);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_meshlets_data_merged_opaque);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_meshlets_data_merged_masked);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_meshlets_data_merged_transparent);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_culled_meshes_data);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_readback_material_gpu);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_readback_material_cpu);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_readback_mesh_gpu);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_readback_mesh_cpu);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_opaque_prefix_sum_work_expansion_mesh);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_masked_prefix_sum_work_expansion_mesh);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_transparent_prefix_sum_work_expansion_mesh);

        render_task_graph.use_persistent_buffer(sun_light_buffer);
        render_task_graph.use_persistent_buffer(point_light_buffer);
        render_task_graph.use_persistent_buffer(spot_light_buffer);

        render_task_graph.add_task(
            daxa::InlineTask::Transfer(MiscellaneousTasks::UPDATE_GLOBALS)
            .writes(shader_globals_buffer)
            .executes([&](daxa::TaskInterface const &ti) {
                context->gpu_metrics[MiscellaneousTasks::UPDATE_GLOBALS]->start(ti.recorder);
                auto alloc = ti.allocator->allocate_fill(context->shader_globals);
                if(alloc.has_value()) {
                    ti.recorder.copy_buffer_to_buffer({
                        .src_buffer = ti.allocator->buffer(),
                        .dst_buffer = ti.get(shader_globals_buffer).ids[0],
                        .src_offset = alloc->buffer_offset,
                        .dst_offset = 0,
                        .size = sizeof(ShaderGlobals),
                    });

                    context->shader_debug_draw_context.update_debug_buffer(context->device, ti.recorder, *ti.allocator);
                }
        }));

        render_task_graph.add_task(
            daxa::InlineTask::Transfer(MiscellaneousTasks::READBACK_RAM)
            .reads(asset_manager->gpu_readback_material_cpu)
            .reads(asset_manager->gpu_readback_mesh_cpu)
            .executes([&](daxa::TaskInterface const &ti) {
                context->gpu_metrics[MiscellaneousTasks::READBACK_RAM]->start(ti.recorder);

                std::memcpy(asset_manager->readback_material.data(), context->device.buffer_host_address(ti.get(asset_manager->gpu_readback_material_cpu).ids[0]).value(), asset_manager->readback_material.size() * sizeof(u32));
                for(u32 i = 0; i < asset_manager->readback_material.size(); i++) { asset_manager->readback_material[i] = (1 << find_msb(asset_manager->readback_material[i])) >> 1; }

                std::memcpy(asset_manager->readback_mesh.data(), context->device.buffer_host_address(ti.get(asset_manager->gpu_readback_mesh_cpu).ids[0]).value(), asset_manager->readback_mesh.size() * sizeof(u32));
                
                context->gpu_metrics[MiscellaneousTasks::READBACK_RAM]->end(ti.recorder);
        }));

        render_task_graph.add_task(
            daxa::InlineTask::Transfer(MiscellaneousTasks::UPDATE_SCENE)
            .writes(shader_globals_buffer)
            .executes([&](daxa::TaskInterface const &ti) {
                context->gpu_metrics[MiscellaneousTasks::UPDATE_SCENE]->start(ti.recorder);
                scene->update_gpu(ti);
                context->gpu_metrics[MiscellaneousTasks::UPDATE_SCENE]->end(ti.recorder);
        }));

        auto u_command = render_task_graph.create_transient_buffer(daxa::TaskTransientBufferInfo {
            .size = s_cast<u32>(glm::max(sizeof(DispatchIndirectStruct), sizeof(DrawIndirectStruct))),
            .name = "command",
        });

        render_task_graph.add_task(
            daxa::InlineTask::Transfer("clear overdraw image")
            .writes(overdraw_image)
            .executes([&](daxa::TaskInterface const &ti) {
                ti.recorder.clear_image({
                    .clear_value = std::array<i32, 4>{0, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
        }));

        render_task_graph.add_task(DrawMeshletsOnlyDepthWriteCommandTask {
            .views = DrawMeshletsOnlyDepthWriteCommandTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_opaque.view(),
                .u_command = u_command
            },
            .context = context,
        });

        render_task_graph.add_task(DrawMeshletsOnlyDepthTask {
            .views = DrawMeshletsOnlyDepthTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_opaque.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_globals = context->shader_globals_buffer.view(),
                .u_command = u_command,
                .u_depth_image = depth_image_d32.view(),
            },
            .context = context,
        });

        render_task_graph.add_task(DrawMeshletsOnlyDepthMaskedWriteCommandTask {
            .views = DrawMeshletsOnlyDepthMaskedWriteCommandTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_masked.view(),
                .u_command = u_command
            },
            .context = context,
        });

        render_task_graph.add_task(DrawMeshletsOnlyDepthMaskedTask {
            .views = DrawMeshletsOnlyDepthMaskedTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_masked.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_globals = context->shader_globals_buffer.view(),
                .u_command = u_command,
                .u_depth_image = depth_image_d32.view(),
            },
            .context = context,
        });

        render_task_graph.add_task(
            daxa::InlineTask::Transfer(VirtualGeometryTasks::CLEAR_DEPTH_IMAGE_U32)
            .writes(depth_image_u32)
            .executes([this](daxa::TaskInterface const &ti) {
                context->gpu_metrics[VirtualGeometryTasks::CLEAR_DEPTH_IMAGE_U32]->start(ti.recorder);
                ti.recorder.clear_image({
                    .clear_value = std::array<u32, 4>{0, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
                context->gpu_metrics[VirtualGeometryTasks::CLEAR_DEPTH_IMAGE_U32]->end(ti.recorder);
        }));

        render_task_graph.add_task(SoftwareRasterizationOnlyDepthWriteCommandTask {
            .views = SoftwareRasterizationOnlyDepthWriteCommandTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_opaque.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(SoftwareRasterizationOnlyDepthTask {
            .views = SoftwareRasterizationOnlyDepthTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_opaque.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_globals = context->shader_globals_buffer.view(),
                .u_depth_image = depth_image_u32.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(CombineDepthTask {
            .views = CombineDepthTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_depth_image_d32 = depth_image_d32.view(),
                .u_depth_image_u32 = depth_image_u32.view(),
                .u_depth_image_f32 = depth_image_f32.view(),
            },
            .context = context,
            .dispatch_callback = [this]() -> daxa::DispatchInfo {
                return { 
                    .x = round_up_div(context->shader_globals.render_target_size.x, 16),
                    .y = round_up_div(context->shader_globals.render_target_size.y, 16),
                    .z = 1
                };
            }
        });

        u32vec2 hiz_size = context->shader_globals.next_lower_po2_render_target_size;
        hiz_size = { std::max(hiz_size.x, 1u), std::max(hiz_size.y, 1u) };
        u32 mip_count = std::max(s_cast<u32>(std::ceil(std::log2(std::max(hiz_size.x, hiz_size.y)))), 1u);
        mip_count = std::min(mip_count, u32(GEN_HIZ_LEVELS_PER_DISPATCH));
        auto hiz = render_task_graph.create_transient_image({
            .format = daxa::Format::R32_SFLOAT,
            .size = {hiz_size.x, hiz_size.y, 1},
            .mip_level_count = mip_count,
            .array_layer_count = 1,
            .sample_count = 1,
            .name = "hiz",
        });

        render_task_graph.add_task(GenerateHizTask{
            .views = GenerateHizTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_src = depth_image_f32.view(),
                .u_mips = hiz,
            },
            .context = context
        });

        render_task_graph.add_task(CullMeshesWriteCommandTask {
            .views = CullMeshesWriteCommandTask::Views {
                .u_scene_data = scene->gpu_scene_data.view(),
                .u_culled_meshes_data = asset_manager->gpu_culled_meshes_data.view(),
                .u_opaque_prefix_sum_work_expansion_mesh = asset_manager->gpu_opaque_prefix_sum_work_expansion_mesh.view(),
                .u_masked_prefix_sum_work_expansion_mesh = asset_manager->gpu_masked_prefix_sum_work_expansion_mesh.view(),
                .u_transparent_prefix_sum_work_expansion_mesh = asset_manager->gpu_transparent_prefix_sum_work_expansion_mesh.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(CullMeshesTask {
            .views = CullMeshesTask::Views {
                .u_scene_data = scene->gpu_scene_data.view(),
                .u_globals = context->shader_globals_buffer.view(),
                .u_entities_data = scene->gpu_entities_data_pool.task_buffer.view(),
                .u_mesh_groups = asset_manager->gpu_mesh_groups.view(),
                .u_mesh_indices = asset_manager->gpu_mesh_indices.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_culled_meshes_data = asset_manager->gpu_culled_meshes_data.view(),
                .u_readback_mesh = asset_manager->gpu_readback_mesh_gpu.view(),
                .u_opaque_prefix_sum_work_expansion_mesh = asset_manager->gpu_opaque_prefix_sum_work_expansion_mesh.view(),
                .u_masked_prefix_sum_work_expansion_mesh = asset_manager->gpu_masked_prefix_sum_work_expansion_mesh.view(),
                .u_transparent_prefix_sum_work_expansion_mesh = asset_manager->gpu_transparent_prefix_sum_work_expansion_mesh.view(),
                .u_hiz = hiz,
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(CullMeshletsOpaqueWriteCommandTask {
            .views = CullMeshletsOpaqueWriteCommandTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_opaque.view(),
                .u_prefix_sum_work_expansion_mesh = asset_manager->gpu_opaque_prefix_sum_work_expansion_mesh.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(CullMeshletsOpaqueTask {
            .views = CullMeshletsOpaqueTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_prefix_sum_work_expansion_mesh = asset_manager->gpu_opaque_prefix_sum_work_expansion_mesh.view(),
                .u_culled_meshes_data = asset_manager->gpu_culled_meshes_data.view(),
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_opaque.view(),
                .u_hiz = hiz,
                .u_command = u_command,
            },
            .context = context,
            .push = {
                .uses = {},
                .force_hw_rasterization = 0,
            },
        });

        render_task_graph.add_task(CullMeshletsMaskedWriteCommandTask {
            .views = CullMeshletsMaskedWriteCommandTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_masked.view(),
                .u_prefix_sum_work_expansion_mesh = asset_manager->gpu_masked_prefix_sum_work_expansion_mesh.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(CullMeshletsMaskedTask {
            .views = CullMeshletsMaskedTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_prefix_sum_work_expansion_mesh = asset_manager->gpu_masked_prefix_sum_work_expansion_mesh.view(),
                .u_culled_meshes_data = asset_manager->gpu_culled_meshes_data.view(),
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_masked.view(),
                .u_hiz = hiz,
                .u_command = u_command,
            },
            .context = context,
            .push = {
                .uses = {},
                .force_hw_rasterization = 1,
            },
        });

        render_task_graph.add_task(CullMeshletsTransparentWriteCommandTask {
            .views = CullMeshletsTransparentWriteCommandTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_transparent.view(),
                .u_prefix_sum_work_expansion_mesh = asset_manager->gpu_transparent_prefix_sum_work_expansion_mesh.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(CullMeshletsTransparentTask {
            .views = CullMeshletsTransparentTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_prefix_sum_work_expansion_mesh = asset_manager->gpu_transparent_prefix_sum_work_expansion_mesh.view(),
                .u_culled_meshes_data = asset_manager->gpu_culled_meshes_data.view(),
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_transparent.view(),
                .u_hiz = hiz,
                .u_command = u_command,
            },
            .context = context,
            .push = {
                .uses = {},
                .force_hw_rasterization = 1,
            },
        });

        render_task_graph.add_task(
            daxa::InlineTask::Transfer(VirtualGeometryTasks::CLEAR_VISIBILITY_IMAGE)
            .writes(visibility_image)
            .executes([this](daxa::TaskInterface const &ti) {
               context->gpu_metrics[VirtualGeometryTasks::CLEAR_VISIBILITY_IMAGE]->start(ti.recorder);
                ti.recorder.clear_image({
                    .clear_value = std::array<u32, 4>{INVALID_ID, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
                context->gpu_metrics[VirtualGeometryTasks::CLEAR_VISIBILITY_IMAGE]->end(ti.recorder);
        }));

        render_task_graph.add_task(DrawMeshletsWriteCommandTask {
            .views = DrawMeshletsWriteCommandTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_opaque.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(DrawMeshletsTask {
            .views = DrawMeshletsTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_opaque.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_globals = context->shader_globals_buffer.view(),
                .u_visibility_image = visibility_image.view(),
                .u_overdraw_image = overdraw_image.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(DrawMeshletsMaskedWriteCommandTask {
            .views = DrawMeshletsMaskedWriteCommandTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_masked.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(DrawMeshletsMaskedTask {
            .views = DrawMeshletsMaskedTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_masked.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_globals = context->shader_globals_buffer.view(),
                .u_visibility_image = visibility_image.view(),
                .u_overdraw_image = overdraw_image.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(SoftwareRasterizationWriteCommandTask {
            .views = SoftwareRasterizationWriteCommandTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_opaque.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(SoftwareRasterizationTask {
            .views = SoftwareRasterizationTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_opaque.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_globals = context->shader_globals_buffer.view(),
                .u_visibility_image = visibility_image.view(),
                .u_overdraw_image = overdraw_image.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(
            daxa::InlineTask::Transfer(PostProcessingTasks::CLEAR_GENERATE_GBUFFER_IMAGES)
            .writes(normal_image)
            .writes(normal_half_res_image)
            .writes(normal_detail_image)
            .writes(normal_detail_half_res_image)
            .writes(depth_half_res_image)
            .executes([this](daxa::TaskInterface const &ti) {
                context->gpu_metrics[PostProcessingTasks::CLEAR_GENERATE_GBUFFER_IMAGES]->start(ti.recorder);
                ti.recorder.clear_image({
                    .clear_value = std::array<u32, 4>{0, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                });
                ti.recorder.clear_image({
                    .clear_value = std::array<u32, 4>{0, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(1)).ids[0],
                });
                ti.recorder.clear_image({
                    .clear_value = std::array<u32, 4>{0, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(2)).ids[0],
                });
                ti.recorder.clear_image({
                    .clear_value = std::array<u32, 4>{0, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(3)).ids[0],
                });
                ti.recorder.clear_image({
                    .clear_value = std::array<f32, 4>{0, 0, 0, 0},
                    .dst_image = ti.get(daxa::TaskImageAttachmentIndex(4)).ids[0],
                });
                context->gpu_metrics[PostProcessingTasks::CLEAR_GENERATE_GBUFFER_IMAGES]->end(ti.recorder);
        }));

        render_task_graph.add_task(ExtractDepthTask {
            .views = ExtractDepthTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_visibility_image = visibility_image.view(),
                .u_depth_image = depth_image_d32.view(),
            },
            .context = context,
        });

        render_task_graph.add_task(GenerateGBufferTask {
            .views = GenerateGBufferTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_meshlets_instance_data = asset_manager->gpu_meshlets_instance_data.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_visibility_image = visibility_image.view(),
                .u_normal_image = normal_image.view(),
                .u_normal_half_res_image = normal_half_res_image.view(),
                .u_normal_detail_image = normal_detail_image.view(),
                .u_normal_detail_half_res_image = normal_detail_half_res_image.view(),
                .u_depth_half_res_image = depth_half_res_image.view(),
            },
            .context = context,
            .dispatch_callback = [this]() -> daxa::DispatchInfo {
                return { 
                    .x = round_up_div(context->shader_globals.render_target_size.x, 8),
                    .y = round_up_div(context->shader_globals.render_target_size.y, 8),
                    .z = 1
                };
            }
        });

        render_task_graph.add_task(GenerateSSAOTask {
            .views = GenerateSSAOTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_normal_detail_half_res_image = normal_detail_half_res_image.view(),
                .u_depth_half_res_image = depth_half_res_image.view(),
                .u_ssao_image = ssao_image.view(),
            },
            .context = context,
            .dispatch_callback = [this]() -> daxa::DispatchInfo {
                return { 
                    .x = round_up_div(context->shader_globals.render_target_half_size.x, 16),
                    .y = round_up_div(context->shader_globals.render_target_half_size.y, 16),
                    .z = 1
                };
            }
        });

        render_task_graph.add_task(CalculateFrustumsTask {
            .views = CalculateFrustumsTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_tile_frustums = tile_frustums_buffer.view(),
            },
            .context = context,
            .dispatch_callback = [this]() -> daxa::DispatchInfo {
                u32vec2 tile_frustums = {
                    round_up_div(context->shader_globals.render_target_size.x, 16),
                    round_up_div(context->shader_globals.render_target_size.y, 16),
                };

                return { 
                    .x = round_up_div(tile_frustums.x, 16),
                    .y = round_up_div(tile_frustums.y, 16),
                    .z = 1
                };
            }
        });

        render_task_graph.add_task(CullLightsTask {
            .views = CullLightsTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_tile_frustums = tile_frustums_buffer.view(),
                .u_tile_data = tile_data_buffer.view(),
                .u_point_lights = point_light_buffer.view(),
                .u_spot_lights = spot_light_buffer.view(),
                .u_depth_image = depth_image_d32.view(),
                .u_overdraw_image = overdraw_image.view(),
            },
            .context = context,
            .dispatch_callback = [this]() -> daxa::DispatchInfo {
                return { 
                    .x = round_up_div(context->shader_globals.render_target_size.x, 16),
                    .y = round_up_div(context->shader_globals.render_target_size.y, 16),
                    .z = 1
                };
            }
        });

        render_task_graph.add_task(ResolveVisibilityBufferTask {
            .views = ResolveVisibilityBufferTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_meshlets_instance_data = asset_manager->gpu_meshlets_instance_data.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_sun = sun_light_buffer.view(),
                .u_point_lights = point_light_buffer.view(),
                .u_spot_lights = spot_light_buffer.view(),
                .u_tile_data = tile_data_buffer.view(),
                .u_readback_material = asset_manager->gpu_readback_material_gpu.view(),
                .u_ssao_image = ssao_image.view(),
                .u_visibility_image = visibility_image.view(),
                .u_overdraw_image = overdraw_image.view(),
                .u_image = render_image.view(),
            },
            .context = context,
            .dispatch_callback = [this]() -> daxa::DispatchInfo {
                return { 
                    .x = round_up_div(context->shader_globals.render_target_size.x, 16),
                    .y = round_up_div(context->shader_globals.render_target_size.y, 16),
                    .z = 1
                };
            }
        });

        render_task_graph.add_task(DrawMeshletsTransparentWriteCommandTask {
            .views = DrawMeshletsTransparentWriteCommandTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_transparent.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(DrawMeshletsTransparentTask {
            .views = DrawMeshletsTransparentTask::Views {
                .u_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged_transparent.view(),
                .u_meshes = asset_manager->gpu_meshes.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_globals = context->shader_globals_buffer.view(),
                .u_sun = sun_light_buffer.view(),
                .u_point_lights = point_light_buffer.view(),
                .u_spot_lights = spot_light_buffer.view(),
                .u_tile_data = tile_data_buffer.view(),
                .u_wboit_accumulation_image = wboit_accumulation_image.view(),
                .u_wboit_reveal_image = wboit_reveal_image.view(),
                .u_depth_image = depth_image_d32.view(),
                .u_overdraw_image = overdraw_image.view(),
                .u_command = u_command,
            },
            .context = context,
        });

        render_task_graph.add_task(ResolveWBOITTask {
            .views = ResolveWBOITTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_color_image = render_image.view(),
                .u_wboit_accumulation_image = wboit_accumulation_image.view(),
                .u_wboit_reveal_image = wboit_reveal_image.view(),
            },
            .context = context
        });

        render_task_graph.add_task(DebugEntityOOBDrawTask {
            .views = DebugEntityOOBDrawTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_image = render_image.view(),
                .u_visibility_image = visibility_image.view(),
            },
            .context = context,
        });

        render_task_graph.add_task(DebugAABBDrawTask {
            .views = DebugAABBDrawTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_image = render_image.view(),
                .u_visibility_image = visibility_image.view(),
            },
            .context = context,
        });

        render_task_graph.add_task(DebugCircleDrawTask {
            .views = DebugCircleDrawTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_image = render_image.view(),
                .u_visibility_image = visibility_image.view(),
            },
            .context = context,
        });

        render_task_graph.add_task(DebugLineDrawTask {
            .views = DebugLineDrawTask::Views {
                .u_globals = context->shader_globals_buffer.view(),
                .u_transforms = scene->gpu_transforms_pool.task_buffer.view(),
                .u_image = render_image.view(),
                .u_visibility_image = visibility_image.view(),
            },
            .context = context,
        });

        // render_task_graph.add_task(DebugImageTask {
        //     .views = DebugImageTask::Views {
        //         .u_globals = context->shader_globals_buffer.view(),
        //         .u_f32_image = ssao_image.view(),
        //         .u_u32_image = normal_image.view(),
        //         .u_color_image = render_image.view(),
        //     },
        //     .context = context,
        //     .dispatch_callback = [this]() -> daxa::DispatchInfo {
        //         return { 
        //             .x = round_up_div(context->shader_globals.render_target_size.x, 16),
        //             .y = round_up_div(context->shader_globals.render_target_size.y, 16),
        //             .z = 1
        //         };
        //     }
        // });

        render_task_graph.add_task(
            daxa::InlineTask{MiscellaneousTasks::IMGUI_DRAW}
            .color_attachment.reads_writes(swapchain_image)
            .executes([&](daxa::TaskInterface const &ti) {
                context->gpu_metrics[MiscellaneousTasks::IMGUI_DRAW]->start(ti.recorder);
                auto size = ti.device.image_info(ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0]).value().size;
                imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0], size.x, size.y);
                context->gpu_metrics[MiscellaneousTasks::IMGUI_DRAW]->end(ti.recorder);
        }));

        render_task_graph.add_task(
            daxa::InlineTask::Transfer(MiscellaneousTasks::READBACK_COPY)
            .writes(asset_manager->gpu_readback_material_cpu)
            .reads(asset_manager->gpu_readback_material_gpu)
            .writes(asset_manager->gpu_readback_mesh_cpu)
            .reads(asset_manager->gpu_readback_mesh_gpu)
            .executes([&](daxa::TaskInterface const &ti) {
                context->gpu_metrics[MiscellaneousTasks::READBACK_COPY]->start(ti.recorder);
                ti.recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = ti.get(asset_manager->gpu_readback_material_gpu).ids[0],
                    .dst_buffer = ti.get(asset_manager->gpu_readback_material_cpu).ids[0],
                    .src_offset = {},
                    .dst_offset = {},
                    .size = context->device.buffer_info(ti.get(asset_manager->gpu_readback_material_gpu).ids[0]).value().size,
                });
                ti.recorder.clear_buffer(daxa::BufferClearInfo {
                    .buffer = ti.get(asset_manager->gpu_readback_material_gpu).ids[0],
                    .offset = {},
                    .size = context->device.buffer_info(ti.get(asset_manager->gpu_readback_material_gpu).ids[0]).value().size,
                    .clear_value = 0
                });

                ti.recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = ti.get(asset_manager->gpu_readback_mesh_gpu).ids[0],
                    .dst_buffer = ti.get(asset_manager->gpu_readback_mesh_cpu).ids[0],
                    .src_offset = {},
                    .dst_offset = {},
                    .size = context->device.buffer_info(ti.get(asset_manager->gpu_readback_mesh_gpu).ids[0]).value().size,
                });
                ti.recorder.clear_buffer(daxa::BufferClearInfo {
                    .buffer = ti.get(asset_manager->gpu_readback_mesh_gpu).ids[0],
                    .offset = {},
                    .size = context->device.buffer_info(ti.get(asset_manager->gpu_readback_mesh_gpu).ids[0]).value().size,
                    .clear_value = 0
                });
                context->gpu_metrics[MiscellaneousTasks::READBACK_COPY]->end(ti.recorder);
        }));

        render_task_graph.submit({});
        render_task_graph.present({});
        render_task_graph.complete({});
    }

    void Renderer::ui_render_start() {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void Renderer::ui_update() {
        ImGui::Begin("Performance Statistics");
        f64 total_time = 0.0;
        auto visit = [&](this auto& self, const PerfomanceCategory& performace_category, bool display_imgui) -> void {
            bool opened = ImGui::TreeNodeEx(performace_category.name.c_str(), 0, "%s", performace_category.name.c_str()) && display_imgui;
            
            for(const auto& counter_name : performace_category.counters) {
                const auto& metric = context->gpu_metrics[counter_name.task_name];
                total_time += metric->time_elapsed;
            }

            for(const auto& perf : performace_category.performance_categories) {
                self(perf, false);
            }
            
            if(opened) {
                for(const auto& counter_name : performace_category.counters) {
                    const auto& metric = context->gpu_metrics[counter_name.task_name];
                    ImGui::Text("%s : %f ms", counter_name.name.data(), metric->time_elapsed);
                }

                for(const auto& perf : performace_category.performance_categories) {
                    self(perf, true);
                }
                ImGui::TreePop();
            }
        };

        for(const auto& perf : performace_metrics) {
            visit(perf, true);
        }

        ImGui::Separator();
        ImGui::Text("total time : %f ms", total_time);
        ImGui::End();

        ImGui::Begin("Memory Usage");
        ImGui::Text("%s", std::format("Total memory usage for images: {} MBs", std::to_string(s_cast<f64>(context->image_memory_usage) / 1024.0 / 1024.0)).c_str());
        ImGui::Text("%s", std::format("Total memory usage for buffers: {} MBs", std::to_string(s_cast<f64>(context->buffer_memory_usage) / 1024.0 / 1024.0)).c_str());
        ImGui::End();

        ImGui::Begin("Material Readback");
        auto& readback_materials = asset_manager->readback_material;
        for(u32 i = 0; i < readback_materials.size(); i++) {
            ImGui::Text("Material %u: requested %ux%u", i, readback_materials[i], readback_materials[i]);
        }
        ImGui::End();

        ImGui::Begin("Texture sizes");
        auto& texture_sizes = asset_manager->texture_sizes;
        for(u32 i = 0; i < texture_sizes.size(); i++) {
            ImGui::Text("Texture %u: %ux%u", i, texture_sizes[i], texture_sizes[i]);
        }
        ImGui::End();

        ImGui::Begin("Mesh readback");
        auto& mesh_readback = asset_manager->readback_mesh;
        for(u32 i = 0; i < asset_manager->mesh_manifest_entries.size(); i++) {
            ImGui::Text("Mesh %u: %u", i, s_cast<i32>((mesh_readback[i / 32u] & (1u << (i % 32u))) != 0));
        }
        ImGui::End();

        ImGui::Begin("Render options");
        ImGui::Text("Framebuffer resolution: %ux%u", context->shader_globals.render_target_size.x, context->shader_globals.render_target_size.y);
        bool camera_option = s_cast<bool>(context->shader_globals.render_as_observer);
        ImGui::Checkbox("Render as observer", &camera_option);
        context->shader_globals.render_as_observer = s_cast<b32>(camera_option);
        ImGui::End();
    }

    void Renderer::ui_render_end() {
        ImGui::Render();
        startup = false;
    }
}