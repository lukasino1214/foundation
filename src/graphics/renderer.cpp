#include "renderer.hpp"
#include "window.hpp"
#include "gpu_context.hpp"
#include <imgui_impl_glfw.h>

#include "tasks/cull_mesh_instances.inl"
#include "tasks/cull_meshlet_instances.inl"
#include "tasks/debug.inl"
#include "tasks/gen_hiz.inl"
#include "gpu_scene.hpp"
#include <asset/asset_manager.hpp>

#include "tasks/draw_opaque_meshlets_only_depth.inl"
#include "tasks/draw_masked_meshlets_only_depth.inl"
#include "tasks/draw_opaque_meshlets.inl"
#include "tasks/draw_masked_meshlets.inl"

#include "tasks/resolve_visibility_buffer.inl"

namespace foundation {
    // TODO: get rid of this
    static void sync_render_target_globals(Renderer & renderer) {
        const auto width = std::max(renderer.window->get_width(), 1u);
        const auto height = std::max(renderer.window->get_height(), 1u);
        renderer.gpu_context->shader_globals.render_target_size = { width, height };
        renderer.gpu_context->shader_globals.render_target_size_inv = {
            1.0f / s_cast<f32>(renderer.gpu_context->shader_globals.render_target_size.x),
            1.0f / s_cast<f32>(renderer.gpu_context->shader_globals.render_target_size.y),
        };
    }

    Renderer::Renderer(NativeWindow* _window, GPUContext* _gpu_context, GPUScene* _gpu_scene, AssetManager* _asset_manager, Scene* _scene) : window{_window}, gpu_context{_gpu_context}, asset_manager{_asset_manager}, scene{_scene}, gpu_scene{_gpu_scene} {
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(window->glfw_handle, true);
        imgui_renderer =  daxa::ImGuiRenderer(daxa::ImGuiRendererInfo {
            .device = gpu_context->device,
            .format = gpu_context->swapchain.get_format(),
            .imgui_context = ImGui::GetCurrentContext()
        });
        task_graph_debug_ui = daxa::TaskGraphDebugUi(daxa::TaskGraphDebugUiInfo {
            .device = gpu_context->device,
            .imgui_renderer = imgui_renderer,
        });
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;
        
        swapchain_image = daxa::TaskImage{{.swapchain_image = true, .name = "swapchain image"}};
        render_image = daxa::TaskImage{{ .name = "render image" }};
        visibility_image = daxa::TaskImage{{ .name = "visibility image" }};
        depth_image = daxa::TaskImage{{ .name = "depth image" }};

        images = {
            render_image,
            depth_image,
            visibility_image,
        };

        frame_buffer_images = {
            {
                {
                    .format = daxa::Format::R8G8B8A8_UNORM,
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = render_image.info().name,
                },
                render_image,
            },
            {
                {
                    .format = daxa::Format::R32_UINT,
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = visibility_image.info().name,
                },
                visibility_image,
            },
            {
                {
                    .format = daxa::Format::D32_SFLOAT,
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
                    .name = depth_image.info().name,
                },
                depth_image,
            },
        };

        recreate_frame_buffer({ window->get_width(), window->get_height() });
        sync_render_target_globals(*this);
        rebuild_task_graph();
    }

    Renderer::~Renderer() {
        for(auto& task_image : images) {
            for(auto image : task_image.get_state().images) {
                gpu_context->device.destroy_image(image);
            }
        }

        for (auto& task_buffer : buffers) {
            for (auto buffer : task_buffer.get_state().buffers) {
                gpu_context->device.destroy_buffer(buffer);
            }
        }

        for(auto& [_, ray_tracing_pipeline_info] : gpu_context->ray_tracing_pipelines) {
            gpu_context->device.destroy_buffer(ray_tracing_pipeline_info->sbt_buffer_id);
        }
    }

    void Renderer::window_resized() {
        PROFILE_SCOPE;

        gpu_context->swapchain.resize();
        recreate_frame_buffer({window->get_width(), window->get_height()});
        sync_render_target_globals(*this);
        rebuild_task_graph();
    }

    void Renderer::recreate_frame_buffer(const glm::uvec2& size) {
        PROFILE_SCOPE;

        fmt::println("resizing to {}x{}", size.x, size.y);

        for (auto& [info, timg] : frame_buffer_images) {
            for(auto image : timg.get_state().images) {
                gpu_context->device.destroy_image(image);
            }

            info.size = { std::max(size.x, 1u), std::max(size.y, 1u), 1 };
            timg.set_images({.images = std::array{gpu_context->device.create_image(info)}});
        }
    }

    void Renderer::compile_pipelines() {
        PROFILE_SCOPE;

        std::vector<std::tuple<std::string, daxa::RasterPipelineCompileInfo2>> rasters = {
            {DrawOpaqueMeshletsOnlyDepthTask::name(), DrawOpaqueMeshletsOnlyDepthTask::pipeline_config_info()},
            {DrawMaskedMeshletsOnlyDepthTask::name(), DrawMaskedMeshletsOnlyDepthTask::pipeline_config_info()},
            {DrawOpaqueMeshletsTask::name(), DrawOpaqueMeshletsTask::pipeline_config_info()},
            {DrawMaskedMeshletsTask::name(), DrawMaskedMeshletsTask::pipeline_config_info()},

            {DebugAABBDrawTask::name(), DebugAABBDrawTask::pipeline_config_info()},
            {DebugCircleDrawTask::name(), DebugCircleDrawTask::pipeline_config_info()},
            {DebugLineDrawTask::name(), DebugLineDrawTask::pipeline_config_info()},
        };

        for (auto [name, info] : rasters) {
            auto compilation_result = this->gpu_context->pipeline_manager.add_raster_pipeline2(info);
            if(compilation_result.is_ok()) {
                fmt::println("SUCCESSFULLY compiled pipeline {}", name);
            } else {
                fmt::println("FAILED to compile pipeline {} with message \n {}", name, compilation_result.message());
            }

            gpu_context->raster_pipelines[name] = compilation_result.value();
        }

        std::vector<std::tuple<std::string, daxa::ComputePipelineCompileInfo2>> computes = {
            {DrawOpaqueMeshletsOnlyDepthWriteCommandTask::name(), DrawOpaqueMeshletsOnlyDepthWriteCommandTask::pipeline_config_info()},
            {DrawMaskedMeshletsOnlyDepthWriteCommandTask::name(), DrawMaskedMeshletsOnlyDepthWriteCommandTask::pipeline_config_info()},
            {DrawOpaqueMeshletsWriteCommandTask::name(), DrawOpaqueMeshletsWriteCommandTask::pipeline_config_info()},
            {DrawMaskedMeshletsWriteCommandTask::name(), DrawMaskedMeshletsWriteCommandTask::pipeline_config_info()},

            {gen_hiz_pipeline_config_info().name, gen_hiz_pipeline_config_info()},
            
            {CullMeshInstancesWriteCommandTask::name(), CullMeshInstancesWriteCommandTask::pipeline_config_info()},
            {CullMeshInstancesTask::name(), CullMeshInstancesTask::pipeline_config_info()},

            {CullMeshletInstancesWriteCommandTask::name(), CullMeshletInstancesWriteCommandTask::pipeline_config_info()},
            {CullMeshletInstancesTask::name(), CullMeshletInstancesTask::pipeline_config_info()},

            {ResolveVisibilityBufferTask::name(), ResolveVisibilityBufferTask::pipeline_config_info()},
        };

        for (auto [name, info] : computes) {
            auto compilation_result = this->gpu_context->pipeline_manager.add_compute_pipeline2(info);
            if(compilation_result.is_ok()) {
                fmt::println("SUCCESSFULLY compiled pipeline {}", name);
            } else {
                fmt::println("FAILED to compile pipeline {} with message \n {}", name, compilation_result.message());
            }

            gpu_context->compute_pipelines[name] = compilation_result.value();
        }

        std::vector<std::tuple<std::string, daxa::RayTracingPipelineCompileInfo2>> ray_traces = {
        };

        for (auto [name, info] : ray_traces) {
            auto compilation_result = this->gpu_context->pipeline_manager.add_ray_tracing_pipeline2(info);
            if(compilation_result.is_ok()) {
                fmt::println("SUCCESSFULLY compiled pipeline {}", name);
            } else {
                fmt::println("FAILED to compile pipeline {} with message \n {}", name, compilation_result.message());
            }

            this->gpu_context->ray_tracing_pipelines[name] = std::make_shared<RayTracingPipelineInfo>();
            this->gpu_context->ray_tracing_pipelines[name]->pipeline = compilation_result.value();
            auto sbt_info = gpu_context->ray_tracing_pipelines[name]->pipeline->create_default_sbt();
            this->gpu_context->ray_tracing_pipelines[name]->sbt = sbt_info.table;
            this->gpu_context->ray_tracing_pipelines[name]->sbt_buffer_id = sbt_info.buffer;
        }
    }

    void Renderer::rebuild_task_graph() {
        PROFILE_SCOPE;

        std::lock_guard<std::mutex> lock(task_graph_mutex);
        render_task_graph = daxa::TaskGraph({
            .device = gpu_context->device,
            .swapchain = gpu_context->swapchain,
            .name = "render task graph",
        });

        render_task_graph.use_persistent_image(swapchain_image);
        render_task_graph.use_persistent_image(render_image);
        render_task_graph.use_persistent_image(visibility_image);
        render_task_graph.use_persistent_image(depth_image);

        render_task_graph.use_persistent_buffer(gpu_context->shader_globals_buffer);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_materials);

        render_task_graph.use_persistent_buffer(gpu_scene->gpu_mesh_group_instance_count);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_mesh_group_instances);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_transforms);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_mesh_instances);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_mesh_instance_indices);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_culled_mesh_instances_data);

        render_task_graph.use_persistent_buffer(gpu_scene->gpu_culled_meshlet_instances);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_opaque_prefix_sum_work_expansion_mesh);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_masked_prefix_sum_work_expansion_mesh);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_blended_prefix_sum_work_expansion_mesh);

        render_task_graph.use_persistent_buffer(gpu_scene->gpu_opaque_culled_meshlet_instances_region);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_masked_culled_meshlet_instances_region);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_blended_culled_meshlet_instances_region);

        render_task_graph.use_persistent_buffer(gpu_scene->gpu_point_lights);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_spot_lights);
        render_task_graph.use_persistent_tlas(gpu_scene->gpu_tlas);

        if(!pipelines_compiled) {
            render_task_graph.add_task(daxa::InlineTask::Transfer("clear image")
                .writes(render_image)
                .executes([&](daxa::TaskInterface const &ti) {
                    ti.recorder.clear_image(daxa::ImageClearInfo{
                        .clear_value = std::array<f32, 4>{1, 0, 0, 0},
                        .dst_image = ti.get(render_image).ids[0]
                    });
                })
            );

            render_task_graph.add_task(daxa::InlineTask::Transfer("blit image to swapchain")
                .reads(render_image)
                .writes(swapchain_image)
                .executes([&](daxa::TaskInterface const &ti) {
                    auto size_x = ti.device.image_info(ti.get(render_image).ids[0]).value().size.x;
                    auto size_y = ti.device.image_info(ti.get(render_image).ids[0]).value().size.y;
                    ti.recorder.blit_image_to_image(daxa::ImageBlitInfo {
                        .src_image = ti.get(render_image).ids[0],
                        .dst_image = ti.get(swapchain_image).ids[0],
                        .src_offsets = {{{0, 0, 0}, {static_cast<i32>(size_x), static_cast<i32>(size_y), 1}}},
                        .dst_offsets = {{{0, 0, 0}, {static_cast<i32>(size_x), static_cast<i32>(size_y), 1}}},
                        .filter = daxa::Filter::NEAREST,
                    });
                })
            );
            
            render_task_graph.add_task(
                daxa::InlineTask{"imgui draw"}
                .color_attachment.reads_writes(swapchain_image)
                .executes([&](daxa::TaskInterface const &ti) {
                    auto size = ti.device.image_info(ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0]).value().size;
                    imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0], size.x, size.y);
            }));

            render_task_graph.submit({});
            render_task_graph.present({});
            render_task_graph.complete({});
            return;
        }

        render_task_graph.add_task(
            daxa::InlineTask::Transfer("update globals")
            .writes(gpu_context->shader_globals_buffer)
            .executes([&](daxa::TaskInterface const &ti) {
                auto alloc = ti.allocator->allocate_fill(gpu_context->shader_globals);
                ti.recorder.copy_buffer_to_buffer({
                    .src_buffer = ti.allocator->buffer(),
                    .dst_buffer = ti.get(gpu_context->shader_globals_buffer).ids[0],
                    .src_offset = alloc->buffer_offset,
                    .dst_offset = 0,
                    .size = sizeof(ShaderGlobals),
                });
                gpu_context->shader_debug_draw_context.update_debug_buffer(gpu_context->device, ti.recorder, *ti.allocator);
        }));

        auto u_command = render_task_graph.create_transient_buffer(daxa::TaskTransientBufferInfo {
            .size = s_cast<u32>(glm::max(sizeof(DispatchIndirectStruct), sizeof(DrawIndirectStruct))),
            .name = "command",
        });

        render_task_graph.add_task(DrawOpaqueMeshletsOnlyDepthWriteCommandTask {
            .views = DrawOpaqueMeshletsOnlyDepthWriteCommandTask::Views {
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_opaque_culled_meshlet_instances_region.view(),
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawOpaqueMeshletsOnlyDepthTask {
            .views = DrawOpaqueMeshletsOnlyDepthTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_opaque_culled_meshlet_instances_region.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_command = u_command,
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawMaskedMeshletsOnlyDepthWriteCommandTask {
            .views = DrawMaskedMeshletsOnlyDepthWriteCommandTask::Views {
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_masked_culled_meshlet_instances_region.view(),
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawMaskedMeshletsOnlyDepthTask {
            .views = DrawMaskedMeshletsOnlyDepthTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_masked_culled_meshlet_instances_region.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_command = u_command,
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawMaskedMeshletsOnlyDepthWriteCommandTask {
            .views = DrawMaskedMeshletsOnlyDepthWriteCommandTask::Views {
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_blended_culled_meshlet_instances_region.view(),
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawMaskedMeshletsOnlyDepthTask {
            .views = DrawMaskedMeshletsOnlyDepthTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_blended_culled_meshlet_instances_region.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_command = u_command,
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
        });

        daxa::TaskImageView first_pass_hiz = {};
        gpu_context->shader_globals.cull_data = fill_cull_data(*gpu_context);
        task_gen_hiz_single_pass({
            .gpu_context = gpu_context,
            .tg = render_task_graph,
            .src = depth_image,
            .globals = gpu_context->shader_globals_buffer.view(),
            .hiz = &first_pass_hiz,
        });

        render_task_graph.add_task(CullMeshInstancesWriteCommandTask {
            .views = CullMeshInstancesWriteCommandTask::Views {
                .u_mesh_group_instance_count = gpu_scene->gpu_mesh_group_instance_count.view(),
                .u_culled_mesh_instances_data = gpu_scene->gpu_culled_mesh_instances_data.view(),
                .u_opaque_prefix_sum_work_expansion_mesh = gpu_scene->gpu_opaque_prefix_sum_work_expansion_mesh.view(),
                .u_masked_prefix_sum_work_expansion_mesh = gpu_scene->gpu_masked_prefix_sum_work_expansion_mesh.view(),
                .u_blended_prefix_sum_work_expansion_mesh = gpu_scene->gpu_blended_prefix_sum_work_expansion_mesh.view(),
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(CullMeshInstancesTask {
            .views = CullMeshInstancesTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_mesh_group_instance_count = gpu_scene->gpu_mesh_group_instance_count.view(),
                .u_mesh_group_instances = gpu_scene->gpu_mesh_group_instances.view(),
                .u_mesh_instance_indices = gpu_scene->gpu_mesh_instance_indices.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_culled_mesh_instances_data = gpu_scene->gpu_culled_mesh_instances_data.view(),
                .u_opaque_prefix_sum_work_expansion_mesh = gpu_scene->gpu_opaque_prefix_sum_work_expansion_mesh.view(),
                .u_masked_prefix_sum_work_expansion_mesh = gpu_scene->gpu_masked_prefix_sum_work_expansion_mesh.view(),
                .u_blended_prefix_sum_work_expansion_mesh = gpu_scene->gpu_blended_prefix_sum_work_expansion_mesh.view(),
                .u_hiz = first_pass_hiz,
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(CullMeshletInstancesWriteCommandTask {
            .views = CullMeshletInstancesWriteCommandTask::Views {
                .u_culled_meshlet_instances_region = gpu_scene->gpu_opaque_culled_meshlet_instances_region.view(),
                .u_prefix_sum_work_expansion_mesh = gpu_scene->gpu_opaque_prefix_sum_work_expansion_mesh.view(),
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(CullMeshletInstancesTask {
            .views = CullMeshletInstancesTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_prefix_sum_work_expansion_mesh = gpu_scene->gpu_opaque_prefix_sum_work_expansion_mesh.view(),
                .u_culled_mesh_instances_data = gpu_scene->gpu_culled_mesh_instances_data.view(),
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_opaque_culled_meshlet_instances_region.view(),
                .u_hiz = first_pass_hiz,
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(CullMeshletInstancesWriteCommandTask {
            .views = CullMeshletInstancesWriteCommandTask::Views {
                .u_culled_meshlet_instances_region = gpu_scene->gpu_masked_culled_meshlet_instances_region.view(),
                .u_prefix_sum_work_expansion_mesh = gpu_scene->gpu_masked_prefix_sum_work_expansion_mesh.view(),
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(CullMeshletInstancesTask {
            .views = CullMeshletInstancesTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_prefix_sum_work_expansion_mesh = gpu_scene->gpu_masked_prefix_sum_work_expansion_mesh.view(),
                .u_culled_mesh_instances_data = gpu_scene->gpu_culled_mesh_instances_data.view(),
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_masked_culled_meshlet_instances_region.view(),
                .u_hiz = first_pass_hiz,
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(CullMeshletInstancesWriteCommandTask {
            .views = CullMeshletInstancesWriteCommandTask::Views {
                .u_culled_meshlet_instances_region = gpu_scene->gpu_blended_culled_meshlet_instances_region.view(),
                .u_prefix_sum_work_expansion_mesh = gpu_scene->gpu_blended_prefix_sum_work_expansion_mesh.view(),
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(CullMeshletInstancesTask {
            .views = CullMeshletInstancesTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_prefix_sum_work_expansion_mesh = gpu_scene->gpu_blended_prefix_sum_work_expansion_mesh.view(),
                .u_culled_mesh_instances_data = gpu_scene->gpu_culled_mesh_instances_data.view(),
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_blended_culled_meshlet_instances_region.view(),
                .u_hiz = first_pass_hiz,
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawOpaqueMeshletsWriteCommandTask {
            .views = DrawOpaqueMeshletsWriteCommandTask::Views {
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_opaque_culled_meshlet_instances_region.view(),
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawOpaqueMeshletsTask {
            .views = DrawOpaqueMeshletsTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_opaque_culled_meshlet_instances_region.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_command = u_command,
                .u_visibility_image = visibility_image.view(),
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawMaskedMeshletsWriteCommandTask {
            .views = DrawMaskedMeshletsWriteCommandTask::Views {
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_masked_culled_meshlet_instances_region.view(),
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawMaskedMeshletsTask {
            .views = DrawMaskedMeshletsTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_masked_culled_meshlet_instances_region.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_command = u_command,
                .u_visibility_image = visibility_image.view(),
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawMaskedMeshletsWriteCommandTask {
            .views = DrawMaskedMeshletsWriteCommandTask::Views {
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_blended_culled_meshlet_instances_region.view(),
                .u_command = u_command,
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DrawMaskedMeshletsTask {
            .views = DrawMaskedMeshletsTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_culled_meshlet_instances_region = gpu_scene->gpu_blended_culled_meshlet_instances_region.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_command = u_command,
                .u_visibility_image = visibility_image.view(),
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
        });

        const auto render_target_size = glm::uvec2{std::max(window->get_width(), 1u), std::max(window->get_height(), 1u)};

        render_task_graph.add_task(ResolveVisibilityBufferTask {
            .views = ResolveVisibilityBufferTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_culled_meshlet_instances = gpu_scene->gpu_culled_meshlet_instances.view(),
                .u_mesh_instances = gpu_scene->gpu_mesh_instances.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_point_lights = gpu_scene->gpu_point_lights.view(),
                .u_spot_lights = gpu_scene->gpu_spot_lights.view(),
                .u_command = u_command,
                .u_tlas = gpu_scene->gpu_tlas.view(),
                .u_visibility_image = visibility_image.view(),
                .u_render_image = render_image.view(),
            },
            .gpu_context = gpu_context,
            .dispatch_info = {
                .x = round_up_div(render_target_size.x, 16),
                .y = round_up_div(render_target_size.y, 16),
                .z = 1
            },
        });

        render_task_graph.add_task(DebugAABBDrawTask {
            .views = DebugAABBDrawTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_render_image = render_image.view(),
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DebugCircleDrawTask {
            .views = DebugCircleDrawTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_render_image = render_image.view(),
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(DebugLineDrawTask {
            .views = DebugLineDrawTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_render_image = render_image.view(),
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
        });

        render_task_graph.add_task(daxa::InlineTask::Transfer("blit image to swapchain")
            .reads(render_image)
            .writes(swapchain_image)
            .executes([&](daxa::TaskInterface const &ti) {
                auto size_x = ti.device.image_info(ti.get(render_image).ids[0]).value().size.x;
                auto size_y = ti.device.image_info(ti.get(render_image).ids[0]).value().size.y;
                ti.recorder.blit_image_to_image(daxa::ImageBlitInfo {
                    .src_image = ti.get(render_image).ids[0],
                    .dst_image = ti.get(swapchain_image).ids[0],
                    .src_offsets = {{{0, 0, 0}, {static_cast<i32>(size_x), static_cast<i32>(size_y), 1}}},
                    .dst_offsets = {{{0, 0, 0}, {static_cast<i32>(size_x), static_cast<i32>(size_y), 1}}},
                    .filter = daxa::Filter::NEAREST,
                });
            })
        );

        render_task_graph.add_task(
            daxa::InlineTask{"imgui draw"}
            .color_attachment.reads_writes(swapchain_image)
            .executes([&](daxa::TaskInterface const &ti) {
                auto size = ti.device.image_info(ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0]).value().size;
                imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0], size.x, size.y);
        }));

        render_task_graph.submit({});
        render_task_graph.present({});
        render_task_graph.complete({});
    }
    
    void Renderer::render() {
        PROFILE_SCOPE;

        {
            PROFILE_ZONE_NAMED(pipelines);
            auto reloaded_result = gpu_context->pipeline_manager.reload_all();
            if (auto* reload_err = daxa::get_if<daxa::PipelineReloadError>(&reloaded_result)) {
                fmt::println("Failed to reload {}", reload_err->message);
            }
            if (daxa::get_if<daxa::PipelineReloadSuccess>(&reloaded_result) != nullptr) {
                fmt::println("Successfully reloaded!");
            }
        }

        daxa::ImageId image = gpu_context->swapchain.acquire_next_image();
        if(image.is_empty()) { return; }
        swapchain_image.set_images({.images = std::span{&image, 1}});
        
        {
            PROFILE_ZONE_NAMED(rendergraph_execute);
            std::lock_guard<std::mutex> lock(task_graph_mutex);
            render_task_graph.execute({});
        }

        {
            PROFILE_ZONE_NAMED(garbage);
            gpu_context->device.collect_garbage();
        }
    }

    void Renderer::ui_render_start() {
        PROFILE_SCOPE;

        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void Renderer::ui_update() {
        PROFILE_SCOPE;
        task_graph_debug_ui.update(render_task_graph);
    }

    void Renderer::ui_render_end() {
        PROFILE_SCOPE;

        ImGui::Render();
    }
}
