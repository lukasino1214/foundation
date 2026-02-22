#include "renderer.hpp"
#include "window.hpp"
#include "gpu_context.hpp"
#include <imgui_impl_glfw.h>

#include "tasks/depth_prepass.inl"
#include "tasks/draw_mesh.inl"
#include "tasks/ray_tracing.inl"
#include "gpu_scene.hpp"

namespace foundation {
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
        depth_image = daxa::TaskImage{{ .name = "depth image" }};

        images = {
            render_image,
            depth_image,
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
                    .format = daxa::Format::D32_SFLOAT,
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
                    .name = depth_image.info().name,
                },
                depth_image,
            },
        };

        recreate_frame_buffer({ window->get_width(), window->get_height() });
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
            {DepthPrepassTask::name(), DepthPrepassTask::pipeline_config_info()},
            {DrawMeshTask::name(), DrawMeshTask::pipeline_config_info()},
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

        std::vector<std::tuple<std::string, daxa::ComputePipelineCompileInfo2>> computes = {};

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
            {RayTraceTask::name(), RayTraceTask::pipeline_config_info()},
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
        render_task_graph.use_persistent_image(depth_image);

        render_task_graph.use_persistent_buffer(gpu_context->shader_globals_buffer);
        render_task_graph.use_persistent_buffer(asset_manager->gpu_materials);

        render_task_graph.use_persistent_buffer(gpu_scene->gpu_point_lights);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_spot_lights);
        render_task_graph.use_persistent_buffer(gpu_scene->gpu_transforms);
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
        }));

        render_task_graph.add_task(DepthPrepassTask {
            .views = DepthPrepassTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
            .asset_manager = asset_manager,
            .scene = scene,
        });

        render_task_graph.add_task(DrawMeshTask {
            .views = DrawMeshTask::Views {
                .u_globals = gpu_context->shader_globals_buffer.view(),
                .u_transforms = gpu_scene->gpu_transforms.view(),
                .u_materials = asset_manager->gpu_materials.view(),
                .u_point_lights = gpu_scene->gpu_point_lights.view(),
                .u_spot_lights = gpu_scene->gpu_spot_lights.view(),
                .u_tlas = gpu_scene->gpu_tlas.view(),
                .u_render_image = render_image.view(),
                .u_depth_image = depth_image.view(),
            },
            .gpu_context = gpu_context,
            .asset_manager = asset_manager,
            .scene = scene,
        });

        // render_task_graph.add_task(RayTraceTask {
        //     .views = RayTraceTask::Views {
        //         .u_globals = gpu_context->shader_globals_buffer.view(),
        //         .u_meshes = asset_manager->gpu_meshes.view(),
        //         .u_materials = asset_manager->gpu_materials.view(),
        //         .u_tlas = gpu_scene->gpu_tlas.view(),
        //         .u_image = render_image.view(),
        //     },
        //     .gpu_context = gpu_context,
        // });

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