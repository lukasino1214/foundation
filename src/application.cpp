#include <application.hpp>
#include <imgui.h>
#include <glm/gtx/string_cast.hpp>
#include "ecs/components.hpp"

namespace foundation {
    Application::Application() : 
        window{1280, 720, "Foundation"},
        context{this->window},
        scripting_engine{&window},
        scene{std::make_shared<Scene>("scene", &context, &window, &scripting_engine, &file_watcher)},
        asset_processor{std::make_unique<AssetProcessor>(&context)},
        thread_pool{std::make_unique<ThreadPool>(std::thread::hardware_concurrency() - 1)},
        asset_manager{std::make_unique<AssetManager>(&context, scene.get(), thread_pool.get(), asset_processor.get())},
        scene_hierarchy_panel{scene.get()} {
        PROFILE_SCOPE;

        scene->update(delta_time);
        context.update_shader_globals(main_camera, observer_camera, { 720, 480 });
        renderer = std::make_unique<Renderer>(&window, &context, scene.get(), asset_manager.get());
        renderer->recreate_framebuffer({ 720, 480 });

        struct CompilePipelinesTask : Task {
            Renderer* renderer = {};
            explicit CompilePipelinesTask(Renderer* renderer)
                : renderer{renderer} { chunk_count = 1; }

            void callback(u32, u32) override {
                renderer->compile_pipelines();
            }
        };

        auto compile_pipelines_task = std::make_shared<CompilePipelinesTask>(renderer.get());
        thread_pool->async_dispatch(compile_pipelines_task);
    
        last_time_point = std::chrono::steady_clock::now();
        main_camera.camera.resize(s_cast<i32>(window.get_width()), s_cast<i32>(window.get_height()));
        observer_camera.camera.resize(s_cast<i32>(window.get_width()), s_cast<i32>(window.get_height()));

        auto add_transform = [](Entity entity) -> LocalTransformComponent* {
            entity.add_component<GlobalTransformComponent>();
            return entity.add_component<LocalTransformComponent>();
        };

        {
            auto entity = scene->create_entity("sponza");
            add_transform(entity);

            LoadManifestInfo manifesto {
                .parent = entity,
                .path = "assets/binary/Sponza/Sponza.bmodel",
            };

            // AssetProcessor::convert_gltf_to_binary("assets/models/Sponza/glTF/Sponza.gltf", "assets/binary/Sponza/Sponza.bmodel");
            asset_manager->load_model(manifesto);
        }

        {
            auto entity = scene->create_entity("bistro");
            add_transform(entity);

            LoadManifestInfo manifesto {
                .parent = entity,
                .path = "assets/binary/Bistro/Bistro.bmodel",
            };

            // AssetProcessor::convert_gltf_to_binary("assets/models/Bistro/Bistro.glb", "assets/binary/Bistro/Bistro.bmodel");
            asset_manager->load_model(manifesto);
        }

        {
            auto cube1 = scene->create_entity("aabb 1");
            {
                auto* tc = add_transform(cube1);
                tc->set_position({0.0f, 0.0, 0.0f});
                
                cube1.add_component<OOBComponent>();
                auto* script = cube1.add_component<ScriptComponent>();
                script->path = "assets/scripts/test.lua";
                file_watcher.watch("assets/scripts/test.lua");
                scripting_engine.init_script(script, cube1, "assets/scripts/test.lua"); 
            }

            auto cube2 = scene->create_entity("aabb 2");
            cube2.handle.child_of(cube1.handle);
            {
                auto* tc = add_transform(cube2);
                tc->set_position({0.0f, 2.5, 0.0f});
                tc->set_scale({0.5f, 0.5, 0.5f});

                cube2.add_component<OOBComponent>();
                auto* script = cube2.add_component<ScriptComponent>();
                script->path = "assets/scripts/test.lua";
                file_watcher.watch("assets/scripts/test.lua");
                scripting_engine.init_script(script, cube2, "assets/scripts/test.lua"); 
            }

            auto cube3 = scene->create_entity("aabb 3");
            cube3.handle.child_of(cube2.handle);
            {
                auto* tc = add_transform(cube3);
                tc->set_position({0.0f, 2.5, 0.0f});
                tc->set_scale({0.5f, 0.5, 0.5f});

                cube3.add_component<OOBComponent>();
                auto* script = cube3.add_component<ScriptComponent>();
                script->path = "assets/scripts/test.lua";
                file_watcher.watch("assets/scripts/test.lua");
                scripting_engine.init_script(script, cube3, "assets/scripts/test.lua"); 
            }

            auto cube4 = scene->create_entity("aabb 4");
            cube4.handle.child_of(cube3.handle);
            {
                auto* tc = add_transform(cube4);
                tc->set_position({0.0f, 2.5, 0.0f});
                tc->set_scale({0.5f, 0.5, 0.5f});
                cube4.add_component<OOBComponent>();
            }
        }

        viewport_panel = ViewportPanel(&context, &window, &renderer->imgui_renderer, [&](const glm::uvec2& size){ renderer->recreate_framebuffer(size); });
        thread_pool->block_on(compile_pipelines_task);
    }

    Application::~Application() {}

    auto Application::run() -> i32 {
        while(!window.window_state->close_requested) {
            PROFILE_FRAME_START("App Run");
            PROFILE_SCOPE;
            auto new_time_point = std::chrono::steady_clock::now();
            this->delta_time = std::chrono::duration_cast<std::chrono::duration<f32, std::chrono::milliseconds::period>>(new_time_point - this->last_time_point).count() * 0.001f;
            this->last_time_point = new_time_point;
            window.update();

            if(window.window_state->resize_requested) {
                renderer->window_resized();
                window.window_state->resize_requested = false;
            }

            {
                PROFILE_SCOPE_NAMED(update_textures);
                asset_manager->update_textures();
                PROFILE_SCOPE_NAMED(record_gpu_load_processing_commands);
                auto commands = asset_processor->record_gpu_load_processing_commands();
                PROFILE_SCOPE_NAMED(record_manifest_update);
                auto cmd_list = asset_manager->record_manifest_update(AssetManager::RecordManifestUpdateInfo {
                    .uploaded_meshes = commands.uploaded_meshes,
                    .uploaded_textures = commands.uploaded_textures
                });

                auto cmd_lists = std::array{std::move(commands.upload_commands), std::move(cmd_list)};
                context.device.submit_commands(daxa::CommandSubmitInfo {
                    .command_lists = cmd_lists
                });
            }

            update();
            renderer->render();
            PROFILE_FRAME_END("App Run");
        }

        return 0;
    }

    void Application::update() {
        PROFILE_SCOPE;

        renderer->ui_render_start();
        ui_update();
        renderer->ui_render_end();

        auto size = context.device.image_info(renderer->render_image.get_state().images[0]).value().size;
        context.update_shader_globals(main_camera, observer_camera, {size.x, size.y});

        scene->update(delta_time);
    }

    void Application::ui_update() {
        {
            ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
            ImGuiID dockspace_id = ImGui::GetID("dockspace");
            ImGui::Begin("dockSpace", nullptr, window_flags);
            ImGui::PopStyleVar(3);
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
            ImGui::End();
        }

        ImGui::Begin("Performance Statistics");
        f64 total_time = 0.0;
        for(auto& [key, metric] : context.gpu_metrics) {
            total_time += metric->time_elapsed;
            ImGui::Text("%s : %f ms", key.data(), metric->time_elapsed);
        }
        ImGui::Text("total time : %f ms", total_time);
        ImGui::End();

        ImGui::Begin("File Browser");
        ImGui::End();

        ImGui::Begin("Asset Manager Statistics");
        ImGui::Text("Mesh Count: %u", s_cast<u32>(asset_manager->mesh_manifest_entries.size()));
        ImGui::Text("Meshlet Count: %u", asset_manager->total_meshlet_count);
        ImGui::Text("Triangle Count: %u", asset_manager->total_triangle_count);
        ImGui::Text("Vertex Count: %u", asset_manager->total_vertex_count);
        ImGui::End();

        scene_hierarchy_panel.draw();
        renderer->ui_update();
        if(context.shader_globals.render_as_observer != 0u) {
            viewport_panel.draw(observer_camera, delta_time, renderer->render_image);
        } else {
            viewport_panel.draw(main_camera, delta_time, renderer->render_image);
        }
    }
}