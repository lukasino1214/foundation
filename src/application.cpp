#include <application.hpp>
#include <imgui.h>
#include <glm/gtx/string_cast.hpp>
#include "ecs/components.hpp"

namespace foundation {
    Application::Application() : 
        window{1280, 720, "foundation"},
        context{this->window},
        scene{std::make_shared<Scene>("scene", &context, &window)},
        asset_processor{std::make_unique<AssetProcessor>(&context)},
        asset_manager{std::make_unique<AssetManager>(&context, scene.get())},
        renderer{&window, &context, scene.get(), asset_manager.get()},
        thread_pool{std::make_unique<ThreadPool>(std::thread::hardware_concurrency() - 1)},
        scene_hierarchy_panel{scene.get()} {


        last_time_point = std::chrono::steady_clock::now();
        camera.camera.resize(static_cast<i32>(window.get_width()), static_cast<i32>(window.get_height()));

        auto add_transform = [](Entity entity) -> LocalTransformComponent* {
            entity.add_component<GlobalTransformComponent>();
            return entity.add_component<LocalTransformComponent>();
        };

        {
            auto entity = scene->create_entity("bistro");
            auto* tc = add_transform(entity);

            LoadManifestInfo manifesto {
                .parent = entity,
                .path = "assets/models/Bistro/Bistro.glb",
                .thread_pool = thread_pool,
                .asset_processor = asset_processor,
            };

            asset_manager->load_model(manifesto);
        }

        {
            auto entity = scene->create_entity("sponza");
            auto* tc = add_transform(entity);

            LoadManifestInfo manifesto {
                .parent = entity,
                .path = "assets/models/Sponza/glTF/Sponza.gltf",
                .thread_pool = thread_pool,
                .asset_processor = asset_processor,
            };

            asset_manager->load_model(manifesto);
        }

        {
            Entity entity = scene->create_entity("aabb 1");
            auto* aabb = entity.add_component<AABBComponent>();
        }

        {
            Entity entity = scene->create_entity("aabb 2");
            auto* aabb = entity.add_component<AABBComponent>();
            aabb->position = { 0.0f, 2.0f, 0.0f };
        }

        {
            Entity entity = scene->create_entity("aabb 3");
            auto* aabb = entity.add_component<AABBComponent>();
            aabb->position = { 0.0f, 4.0f, 0.0f };
        }

        scene->update(delta_time);
    }

    Application::~Application() {

    }

    auto Application::run() -> i32 {
        while(!window.window_state->close_requested) {
            FrameMarkStart("App Run");
            ZoneScoped;
            auto new_time_point = std::chrono::steady_clock::now();
            this->delta_time = std::chrono::duration_cast<std::chrono::duration<float, std::chrono::milliseconds::period>>(new_time_point - this->last_time_point).count() * 0.001f;
            this->last_time_point = new_time_point;
            window.update();

            if(window.window_state->resize_requested) {
                renderer.window_resized();
                window.window_state->resize_requested = false;
            }

            {
                ZoneNamedN(record_gpu_load_processing_commands, "record_gpu_load_processing_commands", true);
                auto commands = asset_processor->record_gpu_load_processing_commands();
                ZoneNamedN(record_manifest_update, "record_manifest_update", true);
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
            renderer.render();
            FrameMarkEnd("App Run");
        }

        return 0;
    }

    void Application::update() {
        ZoneScoped;

        renderer.ui_render_start();
        ui_update();
        renderer.ui_render_end();
            
        glm::mat4 inverse_projection_matrix = glm::inverse(camera.camera.proj_mat);
        glm::mat4 inverse_view_matrix = glm::inverse(camera.camera.view_mat);
        glm::mat4 projection_view_matrix = camera.camera.proj_mat * camera.camera.view_mat;
        glm::mat4 inverse_projection_view = inverse_projection_matrix * inverse_view_matrix;

        this->context.shader_globals.camera_projection_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&camera.camera.proj_mat);
        this->context.shader_globals.camera_inverse_projection_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&inverse_projection_matrix);
        this->context.shader_globals.camera_view_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&camera.camera.view_mat);
        this->context.shader_globals.camera_inverse_view_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&inverse_view_matrix);
        this->context.shader_globals.camera_projection_view_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&projection_view_matrix);
        this->context.shader_globals.camera_inverse_projection_view_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&inverse_projection_view);
        auto size = context.device.info_image(renderer.render_image.get_state().images[0]).value().size;
        this->context.shader_globals.render_target_size = { size.x, size.y };
        // std::cout << "render_target_size: " << context.shader_globals.render_target_size.x << " " << context.shader_globals.render_target_size.y << std::endl;
        this->context.shader_globals.render_target_size_inv = {
            1.0f / s_cast<f32>(context.shader_globals.render_target_size.x),
            1.0f / s_cast<f32>(context.shader_globals.render_target_size.y),
        };
        this->context.shader_globals.next_lower_po2_render_target_size.x = find_next_lower_po2(context.shader_globals.render_target_size.x);
        this->context.shader_globals.next_lower_po2_render_target_size.y = find_next_lower_po2(context.shader_globals.render_target_size.y);
        // std::cout << "next_lower_po2_render_target_size: " << context.shader_globals.next_lower_po2_render_target_size.x << " " << context.shader_globals.next_lower_po2_render_target_size.y << std::endl;
        this->context.shader_globals.next_lower_po2_render_target_size_inv = {
            1.0f / s_cast<f32>(this->context.shader_globals.next_lower_po2_render_target_size.x),
            1.0f / s_cast<f32>(this->context.shader_globals.next_lower_po2_render_target_size.y),
        };

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
            context.shader_globals.frustum_planes[i] = *reinterpret_cast<daxa_f32vec4*>(&planes[i]);
        }

        this->context.shader_globals.camera_position = *reinterpret_cast<daxa_f32vec3*>(&camera.position);

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
        ImGui::Text("test");
        f64 total_time = 0.0;
        for(auto& [key, metric] : context.gpu_metrics) {
            total_time += metric->time_elapsed;
            ImGui::Text("%s : %f ms", key.data(), metric->time_elapsed);
        }
        ImGui::End();

        ImGui::Begin("File Browser");
        ImGui::End();

        scene_hierarchy_panel.draw();

        renderer.ui_update(camera, delta_time, scene_hierarchy_panel);
    }

}