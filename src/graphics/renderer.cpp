#include <graphics/renderer.hpp>
#include <imgui_impl_glfw.h>

#include <graphics/helper.hpp>
#include <ImGuizmo.h>

namespace foundation {
    Renderer::Renderer(NativeWindow* _window, Context* _context) 
        : window{_window}, context{_context} {
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

        images = {
            render_image,
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
        };

        buffers = {};

        recreate_frame_buffer({ window->get_width(), window->get_height() });
        rebuild_task_graph();
    }

    Renderer::~Renderer() {
        for(auto& task_image : images) {
            for(auto image : task_image.get_state().images) {
                context->device.destroy_image(image);
            }
        }

        for (auto& task_buffer : buffers) {
            for (auto buffer : task_buffer.get_state().buffers) {
                context->device.destroy_buffer(buffer);
            }
        }

        for(auto& [_, ray_tracing_pipeline_info] : context->ray_tracing_pipelines) {
            context->device.destroy_buffer(ray_tracing_pipeline_info->sbt_buffer_id);
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
                fmt::println("Failed to reload {}", reload_err->message);
            }
            if (daxa::get_if<daxa::PipelineReloadSuccess>(&reloaded_result) != nullptr) {
                fmt::println("Successfully reloaded!");
            }
        }

        auto image = context->swapchain.acquire_next_image();
        if(image.is_empty()) { return; }
        swapchain_image.set_images({.images = std::span{&image, 1}});
 
        {
            PROFILE_SCOPE_NAMED(rendergraph_execute);
            std::lock_guard<std::mutex> lock(task_graph_mutex);
            render_task_graph.execute({});
        }
        {
            PROFILE_SCOPE_NAMED(garbage);
            context->device.collect_garbage();
        }
    }

    void Renderer::window_resized() {
        context->swapchain.resize();
        recreate_frame_buffer({window->get_width(), window->get_height()});
    }

    void Renderer::recreate_frame_buffer(const glm::uvec2& size) {
        fmt::println("resizing framebuffers from [{}, {}] to [{}, {}]", context->shader_globals.render_target_size.x, context->shader_globals.render_target_size.y, size.x, size.y);

        for (auto &[info, timg] : frame_buffer_images) {
            for(auto image : timg.get_state().images) {
                context->device.destroy_image(image);
            }

            info.size = { std::max(size.x, 1u), std::max(size.y, 1u), 1 };
            timg.set_images({.images = std::array{context->device.create_image(info)}});
        }

        context->shader_globals.render_target_size = { size.x, size.y };
        context->shader_globals.render_target_size_inv = {
            1.0f / s_cast<f32>(context->shader_globals.render_target_size.x),
            1.0f / s_cast<f32>(context->shader_globals.render_target_size.y),
        };

        rebuild_task_graph();
    }

    void Renderer::compile_pipelines() {
        std::vector<std::tuple<std::string_view, daxa::RasterPipelineCompileInfo2>> rasters = {
        };

        for (auto [name, info] : rasters) {
            auto compilation_result = this->context->pipeline_manager.add_raster_pipeline2(info);
            if(compilation_result.is_ok()) {
                fmt::println("SUCCESSFULLY compiled pipeline {}", name);
            } else {
                fmt::println("FAILED to compile pipeline {} with message \n {}", name, compilation_result.message());
            }

            this->context->raster_pipelines[name] = compilation_result.value();
        }

        std::vector<std::tuple<std::string_view, daxa::ComputePipelineCompileInfo2>> computes = {
        };

        for (auto [name, info] : computes) {
            auto compilation_result = this->context->pipeline_manager.add_compute_pipeline2(info);
            if(compilation_result.is_ok()) {
                fmt::println("SUCCESSFULLY compiled pipeline {}", name);
            } else {
                fmt::println("FAILED to compile pipeline {} with message \n {}", name, compilation_result.message());
            }
            
            this->context->compute_pipelines[name] = compilation_result.value();
        }

        std::vector<std::tuple<std::string_view, daxa::RayTracingPipelineCompileInfo2>> ray_traces = {
        };

        for (auto [name, info] : ray_traces) {
            auto compilation_result = this->context->pipeline_manager.add_ray_tracing_pipeline2(info);
            if(compilation_result.is_ok()) {
                fmt::println("SUCCESSFULLY compiled pipeline {}", name);
            } else {
                fmt::println("FAILED to compile pipeline {} with message \n {}", name, compilation_result.message());
            }

            context->ray_tracing_pipelines[name] = std::make_shared<RayTracingPipelineInfo>();
            context->ray_tracing_pipelines[name]->pipeline = compilation_result.value();
            auto sbt_info = context->ray_tracing_pipelines[name]->pipeline->create_default_sbt();
            context->ray_tracing_pipelines[name]->sbt = sbt_info.table;
            context->ray_tracing_pipelines[name]->sbt_buffer_id = sbt_info.buffer;
        }
    }

    void Renderer::rebuild_task_graph() {
        std::lock_guard<std::mutex> lock(task_graph_mutex);
        render_task_graph = daxa::TaskGraph({
            .device = context->device,
            .swapchain = context->swapchain,
            .name = "render task graph",
        });

        auto& shader_globals_buffer = context->shader_globals_buffer;
        render_task_graph.use_persistent_buffer(shader_globals_buffer);

        render_task_graph.use_persistent_image(swapchain_image);
        render_task_graph.use_persistent_image(render_image);

        render_task_graph.add_task(
            daxa::InlineTask::Transfer("update shader globals")
            .writes(shader_globals_buffer)
            .executes([&](daxa::TaskInterface const &ti) {
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
                    .src_image_layout = daxa::ImageLayout::TRANSFER_SRC_OPTIMAL,
                    .dst_image = ti.get(swapchain_image).ids[0],
                    .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
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

    void Renderer::ui_render_start() {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
    }

    void Renderer::ui_update() {
        if(!pipelines_compiled) {
            if(ImGui::Begin("compile pipelines")) {
                ImGui::Text("Compiling pipelines...");
            }
            ImGui::End();
        }
    }

    void Renderer::ui_render_end() {
        ImGui::Render();
        startup = false;
    }
}