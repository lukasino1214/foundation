#include <graphics/renderer.hpp>
#include <imgui_impl_glfw.h>

#include <graphics/helper.hpp>
#include "common/tasks/clear_image.inl"
#include "common/tasks/debug.inl"
#include <graphics/virtual_geometry/virtual_geometry.hpp>

namespace foundation {
    struct MiscellaneousTasks {
        static inline constexpr std::string_view UPDATE_SCENE = "update scene";
        static inline constexpr std::string_view UPDATE_GLOBALS = "update globals";
        static inline constexpr std::string_view READBACK_COPY = "readback copy";
        static inline constexpr std::string_view READBACK_RAM = "readback ram";
        static inline constexpr std::string_view IMGUI_DRAW = "imgui draw";
    };

    Renderer::Renderer(AppWindow* _window, Context* _context, Scene* _scene, AssetManager* _asset_manager) 
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

        images = {
            render_image,
            depth_image_d32,
            depth_image_u32,
            depth_image_f32,
            visibility_image,
            overdraw_image
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
        };

        context->gpu_metrics[ClearImageTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DebugEntityOOBDrawTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DebugAABBDrawTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[MiscellaneousTasks::UPDATE_SCENE] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[MiscellaneousTasks::UPDATE_GLOBALS] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[MiscellaneousTasks::READBACK_COPY] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[MiscellaneousTasks::READBACK_RAM] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[MiscellaneousTasks::IMGUI_DRAW] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        VirtualGeometryTasks::register_gpu_metrics(context);

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

        const auto virtual_geometry_performance_metrics = VirtualGeometryTasks::register_performance_metrics();
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

    void Renderer::window_resized() {
        context->swapchain.resize();
    }

    void Renderer::recreate_framebuffer(const glm::uvec2& size) {
        for (auto &[info, timg] : frame_buffer_images) {
            for(auto image : timg.get_state().images) {
                context->destroy_image(image);
            }

            info.size = { std::max(size.x, 1u), std::max(size.y, 1u), 1 };

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
        std::vector<std::tuple<std::string_view, daxa::RasterPipelineCompileInfo>> rasters = {
            {DebugEntityOOBDrawTask::name(), DebugEntityOOBDrawTask::pipeline_config_info()},
            {DebugAABBDrawTask::name(), DebugAABBDrawTask::pipeline_config_info()}
        };
        auto virtual_geometry_rasters = VirtualGeometryTasks::get_raster_pipelines();
        rasters.insert(rasters.end(), virtual_geometry_rasters.begin(), virtual_geometry_rasters.end());

        for (auto [name, info] : rasters) {
            auto compilation_result = this->context->pipeline_manager.add_raster_pipeline(info);
            std::cout << std::string{name} + " " << compilation_result.to_string() << std::endl;
            this->context->raster_pipelines[name] = compilation_result.value();
        }

        std::vector<std::tuple<std::string_view, daxa::ComputePipelineCompileInfo>> computes = {
            {ClearImageTask::name(), ClearImageTask::pipeline_config_info()},
        };
        auto virtual_geometry_computes = VirtualGeometryTasks::get_compute_pipelines();
        computes.insert(computes.end(), virtual_geometry_computes.begin(), virtual_geometry_computes.end());

        for (auto [name, info] : computes) {
            auto compilation_result = this->context->pipeline_manager.add_compute_pipeline(info);
            std::cout << std::string{name} + " " << compilation_result.to_string() << std::endl;
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

        render_task_graph.use_persistent_buffer(shader_globals_buffer);
        render_task_graph.use_persistent_buffer(scene->gpu_transforms_pool.task_buffer);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_materials);
        render_task_graph.use_persistent_buffer(scene->gpu_scene_data);
        render_task_graph.use_persistent_buffer(scene->gpu_entities_data_pool.task_buffer);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_mesh_groups);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_mesh_indices);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_meshes);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_meshlets_data_merged);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_culled_meshes_data);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_readback_material_gpu);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_readback_material_cpu);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_readback_mesh_gpu);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_readback_mesh_cpu);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_prefix_sum_work_expansion_mesh);

        render_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, shader_globals_buffer),
            },
            .task = [&](daxa::TaskInterface const &ti) {
                context->gpu_metrics[MiscellaneousTasks::UPDATE_GLOBALS]->start(ti.recorder);
                auto alloc = ti.allocator->allocate_fill(context->shader_globals).value();
                ti.recorder.copy_buffer_to_buffer({
                    .src_buffer = ti.allocator->buffer(),
                    .dst_buffer = ti.get(shader_globals_buffer).ids[0],
                    .src_offset = alloc.buffer_offset,
                    .dst_offset = 0,
                    .size = sizeof(ShaderGlobals),
                });
                context->debug_draw_context.update_debug_buffer(context->device, ti.recorder, *ti.allocator);
                context->gpu_metrics[MiscellaneousTasks::UPDATE_GLOBALS]->end(ti.recorder);
            },
            .name = std::string{MiscellaneousTasks::UPDATE_GLOBALS},
        });

        render_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_READ, asset_manager->gpu_readback_material_cpu),
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_READ, asset_manager->gpu_readback_mesh_cpu),
            },
            .task = [&](daxa::TaskInterface const &ti) {
                context->gpu_metrics[MiscellaneousTasks::READBACK_RAM]->start(ti.recorder);

                std::memcpy(asset_manager->readback_material.data(), context->device.buffer_host_address(ti.get(asset_manager->gpu_readback_material_cpu).ids[0]).value(), asset_manager->readback_material.size() * sizeof(u32));
                for(u32 i = 0; i < asset_manager->readback_material.size(); i++) { asset_manager->readback_material[i] = (1 << find_msb(asset_manager->readback_material[i])) >> 1;}
                
                std::memcpy(asset_manager->readback_mesh.data(), context->device.buffer_host_address(ti.get(asset_manager->gpu_readback_mesh_cpu).ids[0]).value(), asset_manager->readback_mesh.size() * sizeof(u32));
                
                context->gpu_metrics[MiscellaneousTasks::READBACK_RAM]->end(ti.recorder);
            },
            .name = std::string{MiscellaneousTasks::READBACK_RAM},
        });

        render_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, scene->gpu_transforms_pool.task_buffer),
            },
            .task = [&](daxa::TaskInterface const &ti) {
                context->gpu_metrics[MiscellaneousTasks::UPDATE_SCENE]->start(ti.recorder);
                scene->update_gpu(ti);
                context->gpu_metrics[MiscellaneousTasks::UPDATE_SCENE]->end(ti.recorder);
            },
            .name = std::string{MiscellaneousTasks::UPDATE_SCENE},
        });

        VirtualGeometryTasks::build_task_graph(VirtualGeometryTasks::Info {
            .context = context,
            .task_graph = render_task_graph,
            .gpu_scene_data = scene->gpu_scene_data,
            .gpu_entities_data = scene->gpu_entities_data_pool.task_buffer,
            .gpu_meshes = asset_manager->gpu_meshes,
            .gpu_materials = asset_manager->gpu_materials,
            .gpu_transforms = scene->gpu_transforms_pool.task_buffer,
            .gpu_mesh_groups = asset_manager->gpu_mesh_groups,
            .gpu_mesh_indices = asset_manager->gpu_mesh_indices,
            .gpu_meshlets_data_merged = asset_manager->gpu_meshlets_data_merged,
            .gpu_culled_meshes_data = asset_manager->gpu_culled_meshes_data,
            .gpu_readback_material = asset_manager->gpu_readback_material_gpu,
            .gpu_readback_mesh = asset_manager->gpu_readback_mesh_gpu,
            .gpu_prefix_sum_work_expansion_mesh = asset_manager->gpu_prefix_sum_work_expansion_mesh,
            .color_image = render_image,
            .depth_image_d32 = depth_image_d32,
            .depth_image_u32 = depth_image_u32,
            .depth_image_f32 = depth_image_f32,
            .visibility_image = visibility_image,
            .overdraw_image = overdraw_image,
        });

        render_task_graph.add_task(DebugEntityOOBDrawTask {
            .views = std::array{
                DebugEntityOOBDrawTask::AT.u_globals | context->shader_globals_buffer,
                DebugEntityOOBDrawTask::AT.u_transforms | scene->gpu_transforms_pool.task_buffer,
                DebugEntityOOBDrawTask::AT.u_image | render_image,
                DebugEntityOOBDrawTask::AT.u_visibility_image | visibility_image,
            },
            .context = context,
        });

        render_task_graph.add_task(DebugAABBDrawTask {
            .views = std::array{
                DebugAABBDrawTask::AT.u_globals | context->shader_globals_buffer,
                DebugAABBDrawTask::AT.u_transforms | scene->gpu_transforms_pool.task_buffer,
                DebugAABBDrawTask::AT.u_image | render_image,
                DebugAABBDrawTask::AT.u_visibility_image | visibility_image,
            },
            .context = context,
        });

        render_task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, swapchain_image)},
            .task = [&](daxa::TaskInterface ti) {
                context->gpu_metrics[MiscellaneousTasks::IMGUI_DRAW]->start(ti.recorder);
                auto size = ti.device.image_info(ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0]).value().size;
                imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0], size.x, size.y);
                context->gpu_metrics[MiscellaneousTasks::IMGUI_DRAW]->end(ti.recorder);
            },
            .name = std::string{MiscellaneousTasks::IMGUI_DRAW},
        });

        render_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, asset_manager->gpu_readback_material_cpu),
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_READ, asset_manager->gpu_readback_material_gpu),
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, asset_manager->gpu_readback_mesh_cpu),
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_READ, asset_manager->gpu_readback_mesh_gpu),
            },
            .task = [&](daxa::TaskInterface const &ti) {
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
            },
            .name = std::string{MiscellaneousTasks::READBACK_COPY},
        });

        render_task_graph.submit({});
        render_task_graph.present({});
        render_task_graph.complete({});
    }

    void Renderer::ui_render_start() {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void Renderer::ui_update() {
        ImGui::Begin("Performance Statistics");
        f64 total_time = 0.0f;
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
            ImGui::Text("Mesh %u: %u", i, (mesh_readback[i / 32u] & (1u << (i % 32u))) != 0);
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