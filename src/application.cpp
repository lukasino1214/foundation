#include "application.hpp"

#include <ecs/entity.hpp>

namespace foundation {
    Application::Application() :
        thread_pool(std::thread::hardware_concurrency() - 1),
        window("foundation", 1280, 720),
        gpu_context(&window),
        scene{std::make_shared<Scene>("default scene")},
        asset_processor{&gpu_context},
        asset_manager{&gpu_context, &asset_processor, &thread_pool, scene.get()},
        gpu_scene{&gpu_context, scene.get(), &asset_manager},
        renderer{&window, &gpu_context, &gpu_scene, &asset_manager, scene.get()} {
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

        // for(u32 x = 0; x < 16; x++) {
        //     for(u32 y = 0; y < 16; y++) {
        //         auto entity = scene->create_entity(fmt::format("point light {} {}", x, y));
        //         entity.add_component<TransformComponent>();
        //         entity.set_local_position({x, -1, y});

        //         entity.add_component<PointLightComponent>();
        //         auto& p = entity.get_component<PointLightComponent>();
        //         p.color = { 1.0f, 1.0f, 1.0f };
        //         p.intensity = 1.0f;
        //         p.range = 1.0f;
        //     }
        // }

        // for(u32 x = 0; x < 16; x++) {
        //     for(u32 y = 0; y < 16; y++) {
        //         auto entity = scene->create_entity(fmt::format("spot light {} {}", x, y));
        //         entity.add_component<TransformComponent>();
        //         entity.set_local_position({x, -1, y});

        //         entity.add_component<SpotLightComponent>();
        //         auto& s = entity.get_component<SpotLightComponent>();
        //         s.direction = { 0, -1, 0 };
        //         s.color = { 1.0f, 0.0f, 0.0f };
        //         s.intensity = 1.0f;
        //         s.range = 1.0f;
        //         s.inner_cone_angle = 0.0f;
        //         s.outer_cone_angle = std::numbers::pi_v<f32> / 4.0f;
        //     }
        // }

        u32 bistro_count = 1;

        for(u32 x = 0; x < bistro_count; x++) {
            for(u32 y = 0; y < bistro_count; y++) {
                for(u32 z = 0; z < bistro_count; z++) {
                    auto entity = scene->create_entity(std::format("bistro {} {} {}", x, y, z));
                    entity.add_component<TransformComponent>();
                    entity.set_local_position({200.0f * s_cast<f32>(x) + 10.0f, -100.0f * s_cast<f32>(y) - 2.0f, 200.0f * s_cast<f32>(z)});
                    asset_manager.load_asset("assets/models/Bistro/Bistro.glb", entity);
                    // asset_manager.load_asset("assets/models/bistro-full/bistro.gltf", entity);
                }
            }
        }

        // {
        //     Entity root_entity = scene->create_entity("root entity");
        //     root_entity.add_component<TransformComponent>();
        //     root_entity.set_local_position({10, -2, 0});
        //     // asset_manager.load_asset("assets/models/bistro-full/bistro.gltf", root_entity);
        //     asset_manager.load_asset("assets/models/Bistro/Bistro.glb", root_entity);
        // }

        // {
        //     Entity root_entity = scene->create_entity("root entity 2");
        //     root_entity.add_component<TransformComponent>();
        //     root_entity.set_local_position({100, -2, 0});
        //     // asset_manager.load_asset("assets/models/bistro-full/bistro.gltf", root_entity);
        //     asset_manager.load_asset("assets/models/Bistro/Bistro.glb", root_entity);
        // }

        {
            Entity root_entity = scene->create_entity("root entity 3");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({0, 0, -80});
            asset_manager.load_asset("assets/models/Sponza/glTF/Sponza.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 4");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-20, 0, -80});
            root_entity.set_local_scale({5, 5, 5});
            asset_manager.load_asset("assets/models/ABeautifulGame/glTF/ABeautifulGame.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 5");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-30, 0, -80});
            asset_manager.load_asset("assets/models/AnimatedCube/glTF/AnimatedCube.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 6");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-40, 0, -80});
            asset_manager.load_asset("assets/models/AnimatedMorphCube/glTF/AnimatedMorphCube.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 7");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-50, 0, -80});
            asset_manager.load_asset("assets/models/BoxAnimated/glTF/BoxAnimated.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 8");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-60, 0, -80});
            asset_manager.load_asset("assets/models/BrainStem/glTF/BrainStem.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 9");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-70, 0, -80});
            asset_manager.load_asset("assets/models/CesiumMan/glTF/CesiumMan.gltf", root_entity);
        }

        {
            Entity root_entity = scene->create_entity("root entity 10");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-80, 0, -80});
            asset_manager.load_asset("assets/models/DamagedHelmet/glTF/DamagedHelmet.gltf", root_entity);
        }

        // {
        //     Entity root_entity = scene->create_entity("root entity 11");
        //     root_entity.add_component<TransformComponent>();
        //     root_entity.set_local_position({-90, 0, -80});
        //     asset_manager.load_asset("assets/models/InterpolationTest/glTF/InterpolationTest.gltf", root_entity);
        // }

        {
            Entity root_entity = scene->create_entity("root entity 12");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-100, 0, -80});
            asset_manager.load_asset("assets/models/RiggedFigure/glTF/RiggedFigure.gltf", root_entity);
        }

        // {
        //     Entity root_entity = scene->create_entity("root entity 13");
        //     root_entity.add_component<TransformComponent>();
        //     root_entity.set_local_position({-110, 0, -80});
        //     asset_manager.load_asset("assets/models/RiggedSimple/glTF/RiggedSimple.gltf", root_entity);
        // }

        {
            Entity root_entity = scene->create_entity("root entity 14");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({-120, 0, -80});
            asset_manager.load_asset("assets/models/SciFiHelmet/glTF/SciFiHelmet.gltf", root_entity);
        }

        // {
        //     Entity root_entity = scene->create_entity("root entity 15");
        //     root_entity.add_component<TransformComponent>();
        //     root_entity.set_local_position({-130, 0, -80});
        //     asset_manager.load_asset("assets/models/SimpleSkin/glTF/SimpleSkin.gltf", root_entity);
        // }

        {
            Entity root_entity = scene->create_entity("root entity 16");
            root_entity.add_component<TransformComponent>();
            root_entity.set_local_position({50, 0, -80});
            asset_manager.load_asset("assets/models/main_sponza/NewSponza_Main_glTF_003.gltf", root_entity);
            asset_manager.load_asset("assets/models/pkg_a_curtains/NewSponza_Curtains_glTF.gltf", root_entity);
            asset_manager.load_asset("assets/models/pkg_b_ivy/NewSponza_IvyGrowth_glTF.gltf", root_entity);
        }

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
                gpu_scene.update(cmd_recorder, upload_infos);
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
            if(ImGui::Begin("Renderer settings")) {
                ImGui::Text("Framebuffer resolution: %ux%u", gpu_context.shader_globals.render_target_size.x, gpu_context.shader_globals.render_target_size.y);
                bool camera_option = s_cast<bool>(gpu_context.shader_globals.render_as_observer);
                ImGui::Checkbox("Render as observer", &camera_option);
                gpu_context.shader_globals.render_as_observer = s_cast<b32>(camera_option);
            }
            ImGui::End();

            create_window("Console");
            // create_window("Viewport Panel");
            // create_window("Performance Statistics");
            create_window("File Browser");
            create_window("Scene Hierarchy");
            create_window("Entity Inspector");

            if(ImGui::Begin("Performance Statistics")) {
                ImGui::Text("delta time: %f ms", delta_time * 1000.0);
            }
            ImGui::End();

            if(ImGui::Begin("GPU Scene Statistics")) {
                ImGui::Text("total_mesh_group_count: %zu", gpu_scene.total_mesh_group_count);
                ImGui::Text("total_mesh_count: %zu", gpu_scene.total_mesh_count);
                ImGui::Text("total_opaque_mesh_count: %zu", gpu_scene.total_opaque_mesh_count);
                ImGui::Text("total_masked_mesh_count: %zu", gpu_scene.total_masked_mesh_count);
                ImGui::Text("total_blended_mesh_count: %zu", gpu_scene.total_blended_mesh_count);
                ImGui::Text("total_meshlet_count: %zu", gpu_scene.total_meshlet_count);
                ImGui::Text("total_opaque_meshlet_count: %zu", gpu_scene.total_opaque_meshlet_count);
                ImGui::Text("total_masked_meshlet_count: %zu", gpu_scene.total_masked_meshlet_count);
                ImGui::Text("total_blended_meshlet_count: %zu", gpu_scene.total_blended_meshlet_count);
            }
            ImGui::End();

            if(ImGui::Begin("Memory Usage")) {
                auto mem_report = gpu_context.device.device_memory_report_convenient();
    
                {
                    bool open = (ImGui::CollapsingHeader("Total Buffer VRAM Use:        "));
                    ImGui::SameLine();
                    ImGui::Text("%s", fmt::format("{:>10.2f} mb", s_cast<f32>(mem_report.total_buffer_device_memory_use) / 1024.0f  / 1024.0f).c_str());
                    if (open) {
                        std::stable_sort(mem_report.buffer_list.begin(), mem_report.buffer_list.end(), [](auto& a, auto& b){ return a.size > b.size; });
                        const usize list_len_max = 30;
                        const usize list_len = std::min(list_len_max, mem_report.buffer_list.size());
                        if (ImGui::BeginTable("Buffer Mem Use", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("Name", {});
                            ImGui::TableSetupColumn("Size", {});
                            ImGui::TableSetupColumn("Block Allocated?", {});
                            ImGui::TableHeadersRow();
                            for (usize i = 0; i < list_len; ++i) {
                                auto buf = mem_report.buffer_list[i];
                                const char* name = gpu_context.device.buffer_info(buf.id).value().name.data();
        
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("%s", name);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%fmb", s_cast<f64>(buf.size) / 1024.0 / 1024.0);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text(buf.block_allocated ? "Yes" : "No");
                            }
                        }
                        ImGui::EndTable();
                    }
                }
                {
                    bool open = (ImGui::CollapsingHeader("Total Image VRAM Use:         "));
                    ImGui::SameLine();
                    ImGui::Text("%s", fmt::format("{:>10.2f} mb", s_cast<f32>(mem_report.total_image_device_memory_use) / 1024.0f  / 1024.0f).c_str());
                    if (open) {
                        std::stable_sort(mem_report.image_list.begin(), mem_report.image_list.end(), [](auto& a, auto& b){ return a.size > b.size; });
                        const usize list_len_max = 30;
                        const usize list_len = std::min(list_len_max, mem_report.image_list.size());
                        if (ImGui::BeginTable("Image Mem Use", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("Name", {});
                            ImGui::TableSetupColumn("Size", {});
                            ImGui::TableSetupColumn("Block Allocated?", {});
                            ImGui::TableHeadersRow();
                            for (usize i = 0; i < list_len; ++i) {
                                auto img = mem_report.image_list[i];
                                const char* name = gpu_context.device.image_info(img.id).value().name.data();
        
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("%s", name);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%fmb", s_cast<f64>(img.size) / 1024.0 / 1024.0);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text(img.block_allocated ? "Yes" : "No");
                            }
                        }
                        ImGui::EndTable();
                    }
                }
                {
                    bool open = (ImGui::CollapsingHeader("Total Aliased Tlas VRAM Use:   "));
                    ImGui::SameLine();
                    ImGui::Text("%s", fmt::format("{:>10.2f} mb", s_cast<f32>(mem_report.total_aliased_tlas_device_memory_use) / 1024.0f  / 1024.0f).c_str());
                    if (open) {
                        std::stable_sort(mem_report.tlas_list.begin(), mem_report.tlas_list.end(), [](auto& a, auto& b){ return a.size > b.size; });
                        const usize list_len_max = 30;
                        const usize list_len = std::min(list_len_max, mem_report.tlas_list.size());
                        if (ImGui::BeginTable("Tlas Aliased Mem Use", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("Name", {});
                            ImGui::TableSetupColumn("Size", {});
                            ImGui::TableSetupColumn("Buffer Allocated?", {});
                            ImGui::TableHeadersRow();
                            for (usize i = 0; i < list_len; ++i) {
                                auto tlas = mem_report.tlas_list[i];
                                const char* name = gpu_context.device.tlas_info(tlas.id).value().name.data();
        
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("%s", name);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%fmb", s_cast<f64>(tlas.size) / 1024.0 / 1024.0);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("Yes");
                            }
                        }
                        ImGui::EndTable();
                    }
                }
                {
                    bool open = (ImGui::CollapsingHeader("Total Aliased Blas VRAM Use:   "));
                    ImGui::SameLine();
                    ImGui::Text("%s", fmt::format("{:>10.2f} mb", s_cast<f32>(mem_report.total_aliased_blas_device_memory_use) / 1024.0f  / 1024.0f).c_str());
                    if (open) {
                        std::stable_sort(mem_report.blas_list.begin(), mem_report.blas_list.end(), [](auto& a, auto& b){ return a.size > b.size; });
                        const usize list_len_max = 30;
                        const usize list_len = std::min(list_len_max, mem_report.blas_list.size());
                        if (ImGui::BeginTable("Blas Aliased Mem Use", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("Name", {});
                            ImGui::TableSetupColumn("Size", {});
                            ImGui::TableSetupColumn("Buffer Allocated?", {});
                            ImGui::TableHeadersRow();
                            for (usize i = 0; i < list_len; ++i) {
                                auto blas = mem_report.blas_list[i];
                                const char* name = gpu_context.device.blas_info(blas.id).value().name.data();
        
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("%s", name);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%fmb", s_cast<f64>(blas.size) / 1024.0 / 1024.0);
                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("Yes");
                            }
                        }
                        ImGui::EndTable();
                    }
                }
                if (ImGui::CollapsingHeader(fmt::format("Total  Block VRAM Use:        {:>10.2f} mb", s_cast<f32>(mem_report.total_memory_block_device_memory_use) / 1024.0f / 1024.0f ).c_str())) {}
                if (ImGui::CollapsingHeader(fmt::format("Total  VRAM Use:              {:>10.2f} mb", s_cast<f32>(mem_report.total_device_memory_use) / 1024.0f / 1024.0f ).c_str())) {}
            }
            ImGui::End();

            renderer.ui_update();
        }
        
        renderer.ui_render_end();

        scene->update(s_cast<f32>(delta_time));

        if(gpu_context.shader_globals.render_as_observer != 0u) {
            observer_camera.update(window, s_cast<f32>(delta_time));
        } else {
            main_camera.update(window, s_cast<f32>(delta_time));
        }

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
            camera_info.vertical_fov = glm::radians(camera.camera.fov);

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
        if(gpu_context.shader_globals.render_as_observer != 0u) {
            gpu_context.shader_globals.view_camera = set_camera_info(observer_camera);
        } else {
            gpu_context.shader_globals.view_camera = set_camera_info(main_camera);
        }

        gpu_context.shader_globals.render_target_size = { window.get_width(), window.get_height() };
        gpu_context.shader_globals.render_target_size_inv = {
            1.0f / s_cast<f32>(gpu_context.shader_globals.render_target_size.x),
            1.0f / s_cast<f32>(gpu_context.shader_globals.render_target_size.y),
        };
        gpu_context.shader_globals.sun_direction = glm::normalize(f32vec3{0.1, 1.0, 0.1});
    }
}