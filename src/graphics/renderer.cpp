#include <graphics/renderer.hpp>
#include <imgui_impl_glfw.h>

#include <graphics/helper.hpp>
#include "common/tasks/clear_image.inl"
#include "traditional/tasks/triangle.inl"
#include "traditional/tasks/render_meshes.inl"
#include "path_tracing/tasks/raytrace.inl"

namespace Shaper {
    Renderer::Renderer(AppWindow* _window, Context* _context, Scene* _scene) 
        : window{_window}, context{_context}, scene{_scene} {
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

        recreate_framebuffer();

        rebuild_task_graph();

        context->shader_globals.linear_sampler = context->device.create_sampler(daxa::SamplerInfo {
            .address_mode_u = daxa::SamplerAddressMode::REPEAT,
            .address_mode_v = daxa::SamplerAddressMode::REPEAT,
            .address_mode_w = daxa::SamplerAddressMode::REPEAT,
            .name = "linear repeat sampler",
        });

        context->shader_globals.nearest_sampler = context->device.create_sampler(daxa::SamplerInfo {
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
    }

    Renderer::~Renderer() {
        for(auto& task_image : images) {
            for(auto image : task_image.get_state().images) {
                context->device.destroy_image(image);
            }
        }

        for (auto& task_buffer : buffers) {
            for (auto buffer : task_buffer.get_state().buffers) {
                this->context->device.destroy_buffer(buffer);
            }
        }

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        context->device.destroy_sampler(std::bit_cast<daxa::SamplerId>(context->shader_globals.linear_sampler));
        context->device.destroy_sampler(std::bit_cast<daxa::SamplerId>(context->shader_globals.nearest_sampler));

        this->context->device.wait_idle();
        this->context->device.collect_garbage();
    }

    void Renderer::render() {
        auto reloaded_result = context->pipeline_manager.reload_all();
        if (auto* reload_err = daxa::get_if<daxa::PipelineReloadError>(&reloaded_result)) {
            std::cout << "Failed to reload " << reload_err->message << '\n';
        }
        if (daxa::get_if<daxa::PipelineReloadSuccess>(&reloaded_result) != nullptr) {
            std::cout << "Successfully reloaded!\n";
        }

        auto image = context->swapchain.acquire_next_image();
        if(image.is_empty()) { return; }
        swapchain_image.set_images({.images = std::span{&image, 1}});

        std::memcpy(context->device.get_host_address(context->shader_globals_buffer.get_state().buffers[0]).value(), &context->shader_globals, sizeof(ShaderGlobals));
        
        render_task_graph.execute({});
        context->device.collect_garbage();
    }

    void Renderer::window_resized() {
        context->swapchain.resize();

        recreate_framebuffer();
    }

    void Renderer::recreate_framebuffer() {
        for (auto &[info, timg] : frame_buffer_images) {
            if (!timg.get_state().images.empty() && !timg.get_state().images[0].is_empty()) {
                context->device.destroy_image(timg.get_state().images[0]);
            }

            info.size = { std::max(window->get_width(), 1u), std::max(window->get_height(), 1u), 1 };

            timg.set_images({.images = std::array{this->context->device.create_image(info)}});
        }
    }

    void Renderer::compile_pipelines() {
        std::vector<std::tuple<std::string_view, daxa::RasterPipelineCompileInfo>> rasters = {
            {TriangleTask::name(), TriangleTask::pipeline_config_info()},
            {RenderMeshesTask::name(), RenderMeshesTask::pipeline_config_info()}
        };

        for (auto [name, info] : rasters) {
            auto compilation_result = this->context->pipeline_manager.add_raster_pipeline(info);
            std::cout << std::string{name} + " " << compilation_result.to_string() << std::endl;
            this->context->raster_pipelines[name] = compilation_result.value();
        }

        std::vector<std::tuple<std::string_view, daxa::ComputePipelineCompileInfo>> computes = {
            {ClearImageTask::name(), ClearImageTask::pipeline_config_info()},
            {RayTraceTask::name(), RayTraceTask::pipeline_config_info()},
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

        render_task_graph.use_persistent_image(swapchain_image);
        render_task_graph.use_persistent_image(render_image);
        render_task_graph.use_persistent_image(depth_image);
        render_task_graph.use_persistent_buffer(context->shader_globals_buffer);

        build_tradional_task_graph();
        build_virtual_geometry_task_graph();
        build_path_tracing_task_graph();

        render_task_graph.add_task({
            .attachments = {
                daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_WRITE, swapchain_image),
                daxa::inl_attachment(daxa::TaskImageAccess::TRANSFER_READ, render_image)
            },
            .task = [&](daxa::TaskInterface ti) {
                auto& info = context->device.info_image(ti.get(daxa::TaskImageAttachmentIndex(1)).ids[0]).value();

                ti.recorder.blit_image_to_image(daxa::ImageBlitInfo {
                .src_image = ti.get(daxa::TaskImageAttachmentIndex(1)).ids[0],
                .dst_image = ti.get(daxa::TaskImageAttachmentIndex(0)).ids[0],
                .src_slice = {
                    .mip_level = 0,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .src_offsets = {{{0, 0, 0}, {s_cast<i32>(info.size.x), s_cast<i32>(info.size.y), 1}}},
                .dst_slice = {
                    .mip_level = 0,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
                .dst_offsets = {{{0, 0, 0}, {s_cast<i32>(info.size.x), s_cast<i32>(info.size.y), 1}}},
                .filter = daxa::Filter::LINEAR,
            });
            },
            .name = "blit",
        });

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

    void Renderer::ui_update() {
        ImTextureID image = imgui_renderer.create_texture_id(daxa::ImGuiImageContext {
            .image_view_id = render_image.get_state().images[0].default_view(),
            .sampler_id = std::bit_cast<daxa::SamplerId>(context->shader_globals.nearest_sampler)
        });

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        auto viewport_window = [&](const std::string_view& name, Mode mode) {
            ImGui::Begin(name.data());
            if(ImGui::IsWindowFocused()) { switch_mode(mode); }
            ImVec2 size = ImGui::GetContentRegionAvail();
            ImGui::Image(image, size);
            ImGui::End();
        };

        viewport_window("Path Tracing", Mode::PathTracing);
        viewport_window("Virtual Geometry", Mode::VirtualGeomtery);
        viewport_window("Traditional", Mode::Traditional);

        ImGui::PopStyleVar();
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
            .scene = scene
        });
    }

    void Renderer::build_virtual_geometry_task_graph() {
        if(rendering_mode != Mode::VirtualGeomtery) { return; }

        render_task_graph.add_task(ClearImageTask {
            .views = std::array{
                ClearImage::AT.u_globals | context->shader_globals_buffer,
                ClearImage::AT.u_image | render_image
            },
            .context = context,
            .push = { .color = { 0.0f, 1.0f, 0.0f } },
            .dispatch_callback = [this]() -> daxa::DispatchInfo { 
                return daxa::DispatchInfo{
                    round_up_div(window->get_width(), 16),
                    round_up_div(window->get_height(), 16),
                    1
                ,};
            },
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