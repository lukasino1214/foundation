#include <graphics/renderer.hpp>
#include <imgui_impl_glfw.h>

#include <graphics/helper.hpp>
#include "common/tasks/clear_image.inl"
#include "traditional/tasks/triangle.inl"
#include "traditional/tasks/render_meshes.inl"
#include "path_tracing/tasks/raytrace.inl"
#include "virtual_geometry/tasks/populate_meshlets.inl"
#include "virtual_geometry/tasks/draw_meshlets.inl"

#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <math/decompose.hpp>

namespace Shaper {
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

        compile_pipelines();

        swapchain_image = daxa::TaskImage{{.swapchain_image = true, .name = "swapchain image"}};
        render_image = daxa::TaskImage{{ .name = "render image" }};
        depth_image = daxa::TaskImage{{ .name = "depth image" }};

        images = {
            render_image,
            depth_image
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
                    .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_SRC,
                    .name = depth_image.info().name,
                },
                depth_image,
            },
        };

        rebuild_task_graph();

        context->shader_globals.linear_sampler = context->get_sampler(daxa::SamplerInfo {
            .address_mode_u = daxa::SamplerAddressMode::REPEAT,
            .address_mode_v = daxa::SamplerAddressMode::REPEAT,
            .address_mode_w = daxa::SamplerAddressMode::REPEAT,
            .name = "linear repeat sampler",
        });

        context->shader_globals.nearest_sampler = context->get_sampler(daxa::SamplerInfo {
            .magnification_filter = daxa::Filter::NEAREST,
            .minification_filter = daxa::Filter::NEAREST,
            .mipmap_filter = daxa::Filter::NEAREST,
            .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .address_mode_w = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
            .name = "nearest repeat sampler",
        });

        context->gpu_metrics[ClearImageTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[RayTraceTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[TriangleTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[RenderMeshesTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[PopulateMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[PopulateMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsWriteCommandTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
        context->gpu_metrics[DrawMeshletsTask::name()] = std::make_shared<GPUMetric>(context->gpu_metric_pool.get());
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
        ZoneScoped;
        {
            ZoneNamedN(pipelines, "pipelines", true);
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
            ZoneNamedN(rendergraphexecute, "render graph", true);
            render_task_graph.execute({});
        }
        {
            ZoneNamedN(garbage, "garbage", true);
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
    }

    void Renderer::compile_pipelines() {
        std::vector<std::tuple<std::string_view, daxa::RasterPipelineCompileInfo>> rasters = {
            {TriangleTask::name(), TriangleTask::pipeline_config_info()},
            {RenderMeshesTask::name(), RenderMeshesTask::pipeline_config_info()},
            {DrawMeshletsTask::name(), DrawMeshletsTask::pipeline_config_info()}
        };

        for (auto [name, info] : rasters) {
            auto compilation_result = this->context->pipeline_manager.add_raster_pipeline(info);
            std::cout << std::string{name} + " " << compilation_result.to_string() << std::endl;
            this->context->raster_pipelines[name] = compilation_result.value();
        }

        std::vector<std::tuple<std::string_view, daxa::ComputePipelineCompileInfo>> computes = {
            {ClearImageTask::name(), ClearImageTask::pipeline_config_info()},
            {RayTraceTask::name(), RayTraceTask::pipeline_config_info()},
            {PopulateMeshletsWriteCommandTask::name(), PopulateMeshletsWriteCommandTask::pipeline_config_info()},
            {PopulateMeshletsTask::name(), PopulateMeshletsTask::pipeline_config_info()},
            {DrawMeshletsWriteCommandTask::name(), DrawMeshletsWriteCommandTask::pipeline_config_info()},
        };

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
            .name = "render task graph",
        });

        auto& shader_globals_buffer = context->shader_globals_buffer;

        render_task_graph.use_persistent_image(swapchain_image);
        render_task_graph.use_persistent_image(render_image);
        render_task_graph.use_persistent_image(depth_image);
        render_task_graph.use_persistent_buffer(shader_globals_buffer);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_transforms);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_materials);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_scene_data);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_mesh_groups);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_mesh_indices);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_meshes);

        render_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, shader_globals_buffer),
            },
            .task = [&](daxa::TaskInterface const &ti) {
                auto alloc = ti.allocator->allocate_fill(context->shader_globals).value();
                ti.recorder.copy_buffer_to_buffer({
                    .src_buffer = ti.allocator->buffer(),
                    .dst_buffer = ti.get(shader_globals_buffer).ids[0],
                    .src_offset = alloc.buffer_offset,
                    .dst_offset = 0,
                    .size = sizeof(ShaderGlobals),
                });
            },
            .name = "GpuInputUploadTransferTask",
        });

        render_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskBufferAccess::TRANSFER_WRITE, asset_manager->gpu_transforms),
            },
            .task = [&](daxa::TaskInterface const &ti) {
                scene->update_gpu(ti, asset_manager->gpu_transforms);
            },
            .name = "SceneUpdateGPU",
        });

        build_tradional_task_graph();
        build_virtual_geometry_task_graph();
        build_path_tracing_task_graph();

        // render_task_graph.add_task({
        //     .attachments = {
        //         daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, swapchain_image),
        //         daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_READ, render_image)
        //     },
        //     .task = [&](daxa::TaskInterface ti) {
        //         auto& info = context->device.info_image(ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0]).value();

        //         ti.recorder.blit_image_to_image(daxa::ImageBlitInfo {
        //         .src_image = ti.get(daxa::TaskImageAttachmentIndex(1)).ids[0],
        //         .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
        //         .src_slice = {
        //             .mip_level = 0,
        //             .base_array_layer = 0,
        //             .layer_count = 1,
        //         },
        //         .src_offsets = {{{0, 0, 0}, {s_cast<i32>(info.size.x), s_cast<i32>(info.size.y), 1}}},
        //         .dst_slice = {
        //             .mip_level = 0,
        //             .base_array_layer = 0,
        //             .layer_count = 1,
        //         },
        //         .dst_offsets = {{{0, 0, 0}, {s_cast<i32>(info.size.x), s_cast<i32>(info.size.y), 1}}},
        //         .filter = daxa::Filter::LINEAR,
        //     });
        //     },
        //     .name = "blit",
        // });

        render_task_graph.add_task({
            .attachments = {daxa::inl_attachment(daxa::TaskImageAccess::COLOR_ATTACHMENT, swapchain_image)},
            .task = [&](daxa::TaskInterface ti) {
                auto size = ti.device.info_image(ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0]).value().size;
                imgui_renderer.record_commands(ImGui::GetDrawData(), ti.recorder, ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0], size.x, size.y);
            },
            .name = "ImGui Draw",
        });

        render_task_graph.submit({});
        render_task_graph.present({});
        render_task_graph.complete({});
    }

    void Renderer::ui_render_start() {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void Renderer::ui_update(ControlledCamera3D& camera, f32 delta_time, SceneHierarchyPanel& panel) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        auto viewport_window = [&](const std::string_view& name, Mode mode) {
            ImGui::Begin(name.data());
            ImVec2 size = ImGui::GetContentRegionAvail();
            if(viewport_size != *r_cast<glm::vec2*>(&size)) {
                viewport_size = *r_cast<glm::vec2*>(&size);
                camera.camera.resize(static_cast<i32>(size.x), static_cast<i32>(size.y));
                recreate_framebuffer({ size.x, size.y });
            }
            if(ImGui::IsWindowFocused()) { switch_mode(mode); }
            if(ImGui::IsWindowHovered()) { if(window->button_just_pressed(GLFW_MOUSE_BUTTON_1)) { window->capture_cursor(); } }
            if(window->button_just_released(GLFW_MOUSE_BUTTON_1)) { window->release_cursor(); }
            if(rendering_mode == mode) { camera.update(*window, delta_time); }

            ImTextureID image = imgui_renderer.create_texture_id(daxa::ImGuiImageContext {
                .image_view_id = render_image.get_state().images[0].default_view(),
                .sampler_id = std::bit_cast<daxa::SamplerId>(context->shader_globals.nearest_sampler)
            });

            ImGui::Image(image, size);

            f32 length = 24.0f;

            ImVec2 pos = ImGui::GetWindowContentRegionMin();

			pos.x += ImGui::GetWindowPos().x;
			pos.y += ImGui::GetWindowPos().y;

            glm::vec2 origin = *r_cast<glm::vec2*>(&pos) + glm::vec2{size.x - length - 8.0f,  length + 8.0f};

            auto project_axis = [&](const glm::vec3& axis) -> ImVec2 {
                return ImVec2(
                    origin.x + axis.x * length,
                    origin.y + axis.y * length
                );
            };

            auto view_rotation = glm::mat3(camera.camera.view_mat);
            ImVec2 end_x = project_axis(view_rotation * glm::vec3{1.0f, 0.0f, 0.0f});
            ImVec2 end_y = project_axis(view_rotation * glm::vec3{0.0f, -1.0f, 0.0f});
            ImVec2 end_z = project_axis(view_rotation * glm::vec3{0.0f, 0.0f, 1.0f});

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            
            ImU32 color_x = IM_COL32(255, 0, 0, 255);
            draw_list->AddLine(*r_cast<ImVec2*>(&origin), end_x, color_x, 2.0f);
            draw_list->AddText(end_x, color_x, "X");

            ImU32 color_y = IM_COL32(0, 255, 0, 255);
            draw_list->AddLine(*r_cast<ImVec2*>(&origin), end_y, color_y, 2.0f);
            draw_list->AddText(end_y, color_y, "Y");

            ImU32 color_z = IM_COL32(0, 0, 255, 255);
            draw_list->AddLine(*r_cast<ImVec2*>(&origin), end_z, color_z, 2.0f);
            draw_list->AddText(end_z, color_z, "Z");

            // if(!ImGuizmo::IsUsing()) {
            //     if(window->key_just_pressed(Key::U)) {
            //         gizmo_type = -1;
            //     }

            //     if(window->key_just_pressed(Key::I)) {
            //         gizmo_type = ImGuizmo::OPERATION::TRANSLATE;
            //     }

            //     if(window->key_just_pressed(Key::O)) {
            //         gizmo_type = ImGuizmo::OPERATION::ROTATE;
            //     }

            //     if(window->key_just_pressed(Key::P)) {
            //         gizmo_type = ImGuizmo::OPERATION::SCALE;
            //     }
            // }

            // if(panel.selected_entity && gizmo_type != -1) {
            //     ImGuizmo::SetOrthographic(false);
            //     ImGuizmo::SetDrawlist();
            //     ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

            //     glm::mat4 mod_mat = panel.selected_entity.get_component<GlobalTransformComponent>()->model_matrix;
            //     glm::mat4 delta = glm::mat4{1.0};
            //     glm::mat4 proj = camera.camera.proj_mat;
            //     proj[1][1] *= -1.0f;
            //     bool use = ImGuizmo::Manipulate(glm::value_ptr(camera.camera.get_view()), glm::value_ptr(proj), s_cast<ImGuizmo::OPERATION>(gizmo_type), ImGuizmo::WORLD, glm::value_ptr(mod_mat), glm::value_ptr(delta), nullptr, nullptr, nullptr);
            
            //     if(ImGuizmo::IsUsing() && use) {
            //         glm::vec3 translation, rotation, scale;

            //         ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(delta), &translation[0], &rotation[0], &scale[0]);
            //         // math::decompose_transform(delta, translation, rotation, scale);

            //         std::cout << glm::to_string(scale) << std::endl;

            //         auto* gtc = panel.selected_entity.get_component<GlobalTransformComponent>();
            //         auto* ltc = panel.selected_entity.get_component<LocalTransformComponent>();
            //         ltc->position += translation - ltc->position;
            //         ltc->rotation += rotation - ltc->rotation;
            //         ltc->scale += scale - ltc->scale;
            //         ltc->is_dirty = true;
            //     }
            // }


            ImGui::End();
        };

        viewport_window("Path Tracing", Mode::PathTracing);
        viewport_window("Virtual Geometry", Mode::VirtualGeomtery);
        viewport_window("Traditional", Mode::Traditional);

        ImGui::PopStyleVar();

        ImGui::Begin("Memory Usage");
        ImGui::Text("%s", std::format("Total memory usage for images: {} MBs", std::to_string(s_cast<f64>(context->images.total_size) / 1024.0 / 1024.0)).c_str());
        ImGui::Text("%s", std::format("Total memory usage for buffers: {} MBs", std::to_string(s_cast<f64>(context->buffers.total_size) / 1024.0 / 1024.0)).c_str());
        
        // for(auto& resource : context->buffers.resources) {
        //     ImGui::Text("%s", std::format("{} : size in {} bytes", resource.first, std::to_string(resource.second.size)).c_str());
        // }

        // for(auto& resource : context->images.resources) {
        //     ImGui::Text("%s", std::format("{} : size in {} bytes", resource.first, std::to_string(resource.second.size)).c_str());
        // }
        
        ImGui::End();
    }

    void Renderer::ui_render_end() {
        ImGui::Render();
        startup = false;
    }

    void Renderer::switch_mode(Mode mode) {
        if(startup || rendering_mode == mode) { return; }
        rendering_mode = mode;
        switch(mode) {
            case Mode::Traditional: { std::println("Traditional"); break; }
            case Mode::VirtualGeomtery: { std::println("Virtual Geometry"); break; }
            case Mode::PathTracing: { std::println("Path Tracing"); break; }
        }

        rebuild_task_graph();
    }

    void Renderer::build_tradional_task_graph() {
        if(rendering_mode != Mode::Traditional) { return; }

        render_task_graph.add_task(RenderMeshesTask {
            .views = std::array{
                RenderMeshesTask::AT.u_globals | context->shader_globals_buffer,
                RenderMeshesTask::AT.u_image | render_image,
                RenderMeshesTask::AT.u_depth_image | depth_image
            },
            .context = context,
            .scene = scene,
            .asset_manager = asset_manager
        });
    }

    void Renderer::build_virtual_geometry_task_graph() {
        if(rendering_mode != Mode::VirtualGeomtery) { return; }

        auto u_command = render_task_graph.create_transient_buffer(daxa::TaskTransientBufferInfo {
            .size = glm::max(sizeof(DispatchIndirectStruct), sizeof(DrawIndirectStruct)),
            .name = "command",
        });

        auto u_meshlet_data = render_task_graph.create_transient_buffer(daxa::TaskTransientBufferInfo {
            .size = sizeof(MeshletsData),
            .name = "meshlets data",
        });

        render_task_graph.add_task(PopulateMeshletsWriteCommandTask {
            .views = std::array{
                PopulateMeshletsWriteCommandTask::AT.u_scene_data | asset_manager->gpu_scene_data,
                PopulateMeshletsWriteCommandTask::AT.u_command | u_command,

            },
            .context = context,
        });

        render_task_graph.add_task(PopulateMeshletsTask {
            .views = std::array{
                PopulateMeshletsTask::AT.u_scene_data | asset_manager->gpu_scene_data,
                PopulateMeshletsTask::AT.u_mesh_groups | asset_manager->gpu_mesh_groups,
                PopulateMeshletsTask::AT.u_mesh_indices | asset_manager->gpu_mesh_indices,
                PopulateMeshletsTask::AT.u_meshes | asset_manager->gpu_meshes,
                PopulateMeshletsTask::AT.u_command | u_command,
                PopulateMeshletsTask::AT.u_meshlets_data | u_meshlet_data,

            },
            .context = context,
        });

        render_task_graph.add_task(DrawMeshletsWriteCommandTask {
            .views = std::array{
                DrawMeshletsWriteCommandTask::AT.u_meshlets_data | u_meshlet_data,
                DrawMeshletsWriteCommandTask::AT.u_command | u_command,

            },
            .context = context,
        });

        render_task_graph.add_task(DrawMeshletsTask {
            .views = std::array{
                DrawMeshletsTask::AT.u_meshlets_data | u_meshlet_data,
                DrawMeshletsTask::AT.u_meshes | asset_manager->gpu_meshes,
                DrawMeshletsTask::AT.u_transforms | asset_manager->gpu_transforms,
                DrawMeshletsTask::AT.u_materials | asset_manager->gpu_materials,
                DrawMeshletsTask::AT.u_globals | context->shader_globals_buffer,
                DrawMeshletsTask::AT.u_command | u_command,
                DrawMeshletsTask::AT.u_image | render_image,
                DrawMeshletsTask::AT.u_depth_image | depth_image
            },
            .context = context,
        });
    }

    void Renderer::build_path_tracing_task_graph() {
        if(rendering_mode != Mode::PathTracing) { return; }

        render_task_graph.add_task(RayTraceTask {
            .views = std::array{
                RayTrace::AT.u_globals | context->shader_globals_buffer,
                RayTrace::AT.u_image | render_image
            },
            .context = context,
            .dispatch_callback = [this]() -> daxa::DispatchInfo { 
                return daxa::DispatchInfo{
                    round_up_div(window->get_width(), 16),
                    round_up_div(window->get_height(), 16),
                    1
                ,};
            },
        });
    }
}