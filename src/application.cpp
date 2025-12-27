#include <application.hpp>
#include <imgui.h>
#include "ecs/components.hpp"
#include <ecs/entity.hpp>

namespace foundation {
    Application::Application() : 
        window{1280, 720, "Foundation"},
        context{this->window},
        scene{std::make_shared<Scene>("scene", &context, &window)},
        gpu_scene{std::make_unique<GPUScene>(&context, scene.get())},
        asset_processor{std::make_unique<AssetProcessor>(&context)},
        thread_pool{std::make_unique<ThreadPool>(std::thread::hardware_concurrency() - 1)},
        asset_manager{std::make_unique<AssetManager>(&context, scene.get(), thread_pool.get(), asset_processor.get())},
        scene_hierarchy_panel{scene.get()} {
        PROFILE_SCOPE;

        scene->update(delta_time);
        context.update_shader_globals(main_camera, observer_camera, { 720, 480 });
        renderer = std::make_unique<Renderer>(&window, &context, scene.get(), asset_manager.get(), gpu_scene.get());
        renderer->recreate_framebuffer({ 720, 480 });

        struct CompilePipelinesTask : Task {
            Renderer* renderer = {};
            explicit CompilePipelinesTask(Renderer* renderer)
                : renderer{renderer} { chunk_count = 1; }

            void callback(u32 /*chunk_index*/, u32 /*thread_index*/) override {
                renderer->compile_pipelines();
            }
        };

        auto compile_pipelines_task = std::make_shared<CompilePipelinesTask>(renderer.get());
        thread_pool->async_dispatch(compile_pipelines_task);
    
        last_time_point = std::chrono::steady_clock::now();
        main_camera.camera.resize(s_cast<i32>(window.get_width()), s_cast<i32>(window.get_height()));
        observer_camera.camera.resize(s_cast<i32>(window.get_width()), s_cast<i32>(window.get_height()));

        ui_windows.push_back(std::make_unique<FileBrowser>(std::filesystem::current_path()));

#define COOK_ASSETS 0

//         {
//             auto entity = scene->create_entity("sponza");
//             entity.get_handle().add<RootEntityTag>();
//             entity.add_component<TransformComponent>();

//             LoadManifestInfo manifesto {
//                 .parent = entity,
//                 .path = "assets/binary/Sponza/Sponza.bmodel",
//             };
// #if COOK_ASSETS
//             AssetProcessor::convert_gltf_to_binary("assets/models/Sponza/glTF/Sponza.gltf", "assets/binary/Sponza/Sponza.bmodel");
// #else
//             asset_manager->load_model(manifesto);
// #endif
//         }

        {
            auto entity = scene->create_entity("damaged helmet");
            entity.get_handle().add<RootEntityTag>();
            entity.add_component<TransformComponent>();
            entity.set_local_position({-10, 5, 0});
            entity.set_local_rotation({0, 90, 0});

            LoadManifestInfo manifesto {
                .parent = entity,
                .path = "assets/binary/DamagedHelmet/DamagedHelmet.bmodel",
            };
#if COOK_ASSETS
            AssetProcessor::convert_gltf_to_binary("assets/models/DamagedHelmet/glTF/DamagedHelmet.gltf", "assets/binary/DamagedHelmet/DamagedHelmet.bmodel");
#else
            asset_manager->load_model(manifesto);
#endif
        }


        {
            auto entity = scene->create_entity("cubes");
            entity.get_handle().add<RootEntityTag>();
            entity.add_component<TransformComponent>();
            entity.set_local_position({-10, 10, 0});

            LoadManifestInfo manifesto {
                .parent = entity,
                .path = "assets/binary/Cubes/Cubes.bmodel",
            };
#if COOK_ASSETS
            AssetProcessor::convert_gltf_to_binary("assets/models/Cubes/Cubes.gltf", "assets/binary/Cubes/Cubes.bmodel");
#else
            asset_manager->load_model(manifesto);
#endif
        }

#if COOK_ASSETS
        u32 bistro_count = 1;
#else
        u32 bistro_count = 5;
#endif
        for(u32 x = 0; x < bistro_count; x++) {
            for(u32 y = 0; y < bistro_count; y++) {
                for(u32 z = 0; z < bistro_count; z++) {
                    auto entity = scene->create_entity(std::format("bistro {} {} {}", x, y, z));
                    entity.get_handle().add<RootEntityTag>();
                    entity.add_component<TransformComponent>();
                    entity.set_local_position({200.0f * s_cast<f32>(x), 100.0f * s_cast<f32>(y), 200.0f * s_cast<f32>(z)});

                    LoadManifestInfo manifesto {
                        .parent = entity,
                        .path = "assets/binary/Bistro/Bistro.bmodel",
                    };
#if COOK_ASSETS
                    AssetProcessor::convert_gltf_to_binary("assets/models/Bistro/Bistro.glb", "assets/binary/Bistro/Bistro.bmodel");
#else           
                    asset_manager->load_model(manifesto);
#endif                
                }
            }
        }

//         {
//             auto entity = scene->create_entity("small city");
//             entity.get_handle().add<RootEntityTag>();
//             add_transform(entity);

//             LoadManifestInfo manifesto {
//                 .parent = entity,
//                 .path = "assets/binary/small_city/small_city.bmodel",
//             };
// #if COOK_ASSETS
//                     AssetProcessor::convert_gltf_to_binary("assets/models/small_city/small_city.gltf", "assets/binary/small_city/small_city.bmodel");
// #else           
//                     asset_manager->load_model(manifesto);
// #endif      
//         }

        // {
        //     Entity parent = {};
        //     for(u32 index = 0; index < 16; index++) {
        //         Entity entity = scene->create_entity(std::string{"cube "} + std::to_string(index));
        //         entity.add_component<TransformComponent>();
        //         if(index == 0) { entity.get_handle().add<RootEntityTag>(); entity.set_local_scale({10.0f, 10.0, 10.0f}); }
        //         if(index != 0) { parent.set_child(entity); }
        //         entity.set_local_position({0.0f, 0.0, 0.0f});
        //         if(index != 0) { entity.set_local_position({0.0f, 5, 0.0f}); entity.set_local_scale({0.5f, 0.5f, 0.5f}); }
        //         entity.add_component<OOBComponent>();
        //         auto* script = entity.add_component<ScriptComponent>();
        //         script->path = "assets/scripts/test.lua";
        //         file_watcher.watch("assets/scripts/test.lua");
        //         scripting_engine.init_script(script, entity, "assets/scripts/test.lua");
        //         parent = entity;
        //     }
        // }

        viewport_panel = ViewportPanel(&context, &window, &renderer->imgui_renderer, [&](const glm::uvec2& size){ renderer->recreate_framebuffer(size); });
        thread_pool->block_on(compile_pipelines_task);
    }

    Application::~Application() = default;

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
                PROFILE_ZONE_NAMED(update_textures);
                asset_manager->stream_textures();
                PROFILE_ZONE_NAMED(update_meshes);
                asset_manager->stream_meshes();
                PROFILE_ZONE_NAMED(record_gpu_load_processing_commands);
                auto commands = asset_processor->record_gpu_load_processing_commands();
                PROFILE_ZONE_NAMED(record_manifest_update);
                auto update_info = asset_manager->record_manifest_update(AssetManager::RecordManifestUpdateInfo {
                    .uploaded_meshes = commands.uploaded_meshes,
                    .uploaded_textures = commands.uploaded_textures
                });

                auto gpu_scene_cmd_list = gpu_scene->update(GPUScene::UpdateInfo {
                    .update_mesh_groups = update_info.update_mesh_groups,
                    .update_meshes = update_info.update_meshes,
                });

                auto cmd_lists = std::array{std::move(commands.upload_commands), std::move(update_info.command_list), std::move(gpu_scene_cmd_list)};
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
        {
            PROFILE_ZONE_NAMED(ui_render_start);
            renderer->ui_render_start();
        }
        {
            PROFILE_ZONE_NAMED(ui_update_);
            ui_update();
        }
        {
            PROFILE_ZONE_NAMED(ui_render_end);
            renderer->ui_render_end();
        }
        
        {
            PROFILE_ZONE_NAMED(record_manifest_update);
            auto size = context.device.image_info(renderer->render_image.get_state().images[0]).value().size;
            context.update_shader_globals(main_camera, observer_camera, {size.x, size.y});
        }

        {
            PROFILE_ZONE_NAMED(scene_update);
            scene->update(delta_time);
        }
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

        ImGui::Begin("Asset Manager Statistics");
        // ImGui::Text("Unique Mesh Count: %u", s_cast<u32>(asset_manager->total_unique_mesh_count));
        // ImGui::Text("Opaque Mesh Count: %u", s_cast<u32>(asset_manager->total_opaque_mesh_count));
        // ImGui::Text("Masked Mesh Count: %u", s_cast<u32>(asset_manager->total_masked_mesh_count));
        // ImGui::Text("Transparent Mesh Count: %zu", asset_manager->total_transparent_mesh_count);
        // ImGui::Text("Mesh Count: %u", s_cast<u32>(asset_manager->total_mesh_count));
        // ImGui::Text("Meshlet Count: %zu", asset_manager->total_meshlet_count);
        // ImGui::Text("Opaque Meshlet Count: %zu", asset_manager->total_opaque_meshlet_count);
        // ImGui::Text("Masked Meshlet Count: %zu", asset_manager->total_masked_meshlet_count);
        // ImGui::Text("Transparent Meshlet Count: %zu", asset_manager->total_transparent_meshlet_count);
        // ImGui::Text("Triangle Count: %zu", asset_manager->total_triangle_count);
        // ImGui::Text("Vertex Count: %zu", asset_manager->total_vertex_count);
        ImGui::End();

        scene_hierarchy_panel.draw();
        renderer->ui_update();
        viewport_panel.selected_entity = scene_hierarchy_panel.selected_entity;
        if(context.shader_globals.render_as_observer != 0u) {
            viewport_panel.draw(observer_camera, delta_time, renderer->render_image);
        } else {
            viewport_panel.draw(main_camera, delta_time, renderer->render_image);
        }

        for(auto& ui_window : ui_windows) {
            ui_window->draw();
        }

        ui_windows.erase(std::remove_if(ui_windows.begin(), ui_windows.end(), [](std::unique_ptr<UIWindow>& ui_window){ return ui_window->should_close(); }), ui_windows.end());
    }
}