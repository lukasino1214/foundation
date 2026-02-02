#include "application.hpp"

namespace foundation {
    Application::Application() :
        thread_pool(std::thread::hardware_concurrency() - 1),
        window("foundation", 1280, 720),
        gpu_context(&window),
        scene{std::make_shared<Scene>("default scene")},
        asset_processor{&gpu_context},
        asset_manager{&gpu_context, &asset_processor, &thread_pool, scene.get()},
        gpu_scene{scene.get(), &asset_manager},
        renderer{&window, &gpu_context, &asset_manager, scene.get()} {
        PROFILE_SCOPE;

        struct CompilePipelinesTask : Task {
            Renderer* renderer = {};
            explicit CompilePipelinesTask(Renderer* renderer) : renderer{renderer} { chunk_count = 1; }

            void callback(u32 /*chunk_index*/, u32 /*thread_index*/) override {
                renderer->compile_pipelines();
                renderer->pipelines_compiled = true;
                renderer->rebuild_task_graph();
            }
        };

        auto compile_pipelines_task = std::make_shared<CompilePipelinesTask>(&renderer);
        thread_pool.async_dispatch(compile_pipelines_task);

        {
            Entity root_entity = scene->create_entity("root entity");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({10, -2, 0});
            asset_manager.load_asset("assets/models/bistro-full/bistro.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 2");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({100, -2, 0});
            asset_manager.load_asset("assets/models/bistro-full/bistro.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 3");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({0, 50, 0});
            asset_manager.load_asset("assets/models/Sponza/glTF/Sponza.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 4");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-20, 50, 0});
            root_entity.set_local_scale({5, 5, 5});
            asset_manager.load_asset("assets/models/ABeautifulGame/glTF/ABeautifulGame.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 5");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-30, 50, 0});
            asset_manager.load_asset("assets/models/AnimatedCube/glTF/AnimatedCube.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 6");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-40, 50, 0});
            asset_manager.load_asset("assets/models/AnimatedMorphCube/glTF/AnimatedMorphCube.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 7");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-50, 50, 0});
            asset_manager.load_asset("assets/models/BoxAnimated/glTF/BoxAnimated.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 8");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-60, 50, 0});
            asset_manager.load_asset("assets/models/BrainStem/glTF/BrainStem.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 9");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-70, 50, 0});
            asset_manager.load_asset("assets/models/CesiumMan/glTF/CesiumMan.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 10");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-80, 50, 0});
            asset_manager.load_asset("assets/models/DamagedHelmet/glTF/DamagedHelmet.gltf", root_entity);
        }

        // {
        //     Entity root_entity = scene->create_entity("root entity 11");
        //     root_entity.add_component<TransformComponent>();
        //     root_entity.set_local_position({-90, 50, 0});
        //     asset_manager.load_asset("assets/models/InterpolationTest/glTF/InterpolationTest.gltf", root_entity);
        // }

        {
            Entity root_entity = scene->create_entity("root entity 12");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-100, 50, 0});
            asset_manager.load_asset("assets/models/RiggedFigure/glTF/RiggedFigure.gltf", root_entity);
        }

        // {
        //     Entity root_entity = scene->create_entity("root entity 13");
        //     root_entity.add_component<TransformComponent>();
        //     root_entity.set_local_position({-110, 50, 0});
        //     asset_manager.load_asset("assets/models/RiggedSimple/glTF/RiggedSimple.gltf", root_entity);
        // }

        {
            Entity root_entity = scene->create_entity("root entity 14");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-120, 50, 0});
            asset_manager.load_asset("assets/models/SciFiHelmet/glTF/SciFiHelmet.gltf", root_entity);
        }

        // {
        //     Entity root_entity = scene->create_entity("root entity 15");
        //     root_entity.add_component<TransformComponent>();
        //     root_entity.set_local_position({-130, 50, 0});
        //     asset_manager.load_asset("assets/models/SimpleSkin/glTF/SimpleSkin.gltf", root_entity);
        // }

        // {
        //     Entity root_entity = scene->create_entity("root entity 16");
        //     root_entity.add_component<TransformComponent>();
        //     root_entity.set_local_position({50, 50, 0});
        //     asset_manager.load_asset("assets/models/main_sponza/NewSponza_Main_glTF_003.gltf", root_entity);
        //     asset_manager.load_asset("assets/models/pkg_a_curtains/NewSponza_Curtains_glTF.gltf", root_entity);
        //     asset_manager.load_asset("assets/models/pkg_b_ivy/NewSponza_IvyGrowth_glTF.gltf", root_entity);
        // }

        // model_data = asset_manager.load_asset("assets/models/Bistro/Bistro.glb", gpu_context);
        // model_data = asset_manager.load_asset("assets/models/zorah/zorah_main_public.gltf", gpu_context);
    }

    Application::~Application() {}

    auto Application::run() -> i32 {
        while(!window.window_state->close_requested) {
            PROFILE_FRAME_START("App Run");
            PROFILE_SCOPE;

            auto new_time_point = std::chrono::steady_clock::now();
            delta_time = std::chrono::duration_cast<std::chrono::duration<f64, std::chrono::milliseconds::period>>(new_time_point - this->last_time_point).count() * 0.001;
            last_time_point = new_time_point;

            window.update();

            if(window.window_state->resize_requested) {
                renderer.window_resized();
                window.window_state->resize_requested = false;
            }

            update();

            {
                PROFILE_SCOPE_NAMED(uploading_resources);
                daxa::CommandRecorder cmd_recorder = gpu_context.device.create_command_recorder({ .name = "upload cmd recorder" });
                auto upload_infos = asset_processor.upload_resources(cmd_recorder);
                asset_manager.update(cmd_recorder, upload_infos);
                gpu_scene.update();
                gpu_context.device.submit_commands({ .command_lists = std::to_array({cmd_recorder.complete_current_commands()}) });
            }

            renderer.render();

            PROFILE_FRAME_END("App Run");
        }

        return 0;
    }

    void Application::update() {
        PROFILE_SCOPE;

        renderer.ui_render_start();

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

        auto create_window = [](std::string_view name) {
            ImGui::Begin(name.data());
            ImGui::Text("%s", name.data());
            ImGui::End();
        };

        if(!window.is_cursor_captured()) {
            create_window("Console");
            // create_window("Viewport Panel");
            // create_window("Performance Statistics");
            create_window("GPU Scene Statistic");
            create_window("File Browser");
            create_window("Scene Hierarchy");
            create_window("Entity Inspector");
            create_window("Renderer settings");

            if(ImGui::Begin("Performance Statistics")) {
                ImGui::Text("delta time: %f ms", delta_time * 1000.0);
            }
            ImGui::End();
        }
        
        renderer.ui_update();
        renderer.ui_render_end();

        scene->update(s_cast<f32>(delta_time));

        main_camera.update(window, s_cast<f32>(delta_time));

        auto set_camera_info = [&](ControlledCamera3D& camera) -> CameraInfo {
            camera.camera.resize(s_cast<i32>(window.get_width()), s_cast<i32>(window.get_height()));
            CameraInfo camera_info {};
            glm::mat4 inverse_projection_matrix = glm::inverse(camera.camera.proj_mat);
            glm::mat4 inverse_view_matrix = glm::inverse(camera.camera.view_mat);
            glm::mat4 projection_view_matrix = camera.camera.proj_mat * camera.camera.view_mat;
            glm::mat4 inverse_projection_view = inverse_projection_matrix * inverse_view_matrix;

            camera_info.projection_matrix =  camera.camera.proj_mat;
            camera_info.inverse_projection_matrix = inverse_projection_matrix;
            camera_info.view_matrix = camera.camera.view_mat;
            camera_info.inverse_view_matrix = inverse_view_matrix;
            camera_info.projection_view_matrix = projection_view_matrix;
            camera_info.inverse_projection_view_matrix = inverse_projection_view;
            camera_info.position = camera.position;

            std::array<glm::vec4, 6> planes = {};
            for (i32 i = 0; i < 4; ++i) { planes[0][i] = projection_view_matrix[i][3] + projection_view_matrix[i][0]; }
            for (i32 i = 0; i < 4; ++i) { planes[1][i] = projection_view_matrix[i][3] - projection_view_matrix[i][0]; }
            for (i32 i = 0; i < 4; ++i) { planes[2][i] = projection_view_matrix[i][3] + projection_view_matrix[i][1]; }
            for (i32 i = 0; i < 4; ++i) { planes[3][i] = projection_view_matrix[i][3] - projection_view_matrix[i][1]; }
            for (i32 i = 0; i < 4; ++i) { planes[4][i] = projection_view_matrix[i][3] + projection_view_matrix[i][2]; }
            for (i32 i = 0; i < 4; ++i) { planes[5][i] = projection_view_matrix[i][3] - projection_view_matrix[i][2]; }
            for (u32 i = 0; i < 6; ++i) {
                planes[i] /= glm::length(glm::vec3(planes[i]));
                planes[i].w = -planes[i].w;
                camera_info.frustum_planes[i] = planes[i];
            }

            return camera_info;
        };

        gpu_context.shader_globals.main_camera = set_camera_info(main_camera);
        gpu_context.shader_globals.render_target_size = { window.get_width(), window.get_height() };
        gpu_context.shader_globals.render_target_size_inv = {
            1.0f / s_cast<f32>(gpu_context.shader_globals.render_target_size.x),
            1.0f / s_cast<f32>(gpu_context.shader_globals.render_target_size.y),
        };
    }
}