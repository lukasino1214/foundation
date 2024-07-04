#include <application.hpp>
#include <imgui.h>
#include <glm/gtx/string_cast.hpp>
#include "ecs/components.hpp"

namespace Shaper {
    Application::Application() : 
        window{1280, 720, "shaper"},
        context{this->window},
        scene{std::make_shared<Scene>("scene", &context, &window)},
        asset_processor{std::make_unique<AssetProcessor>(&context)},
        asset_manager{std::make_unique<AssetManager>(&context, scene.get())},
        renderer{&window, &context, scene.get(), asset_manager.get()},
        thread_pool{std::make_unique<ThreadPool>(3)} {


        last_time_point = std::chrono::steady_clock::now();
        camera.camera.resize(static_cast<i32>(window.get_width()), static_cast<i32>(window.get_height()));

        auto add_transform = [](Entity entity) -> LocalTransformComponent* {
            entity.add_component<GlobalTransformComponent>();
            return entity.add_component<LocalTransformComponent>();
        };

        {
            auto entity = scene->create_entity("sponza");
            auto* tc = add_transform(entity);
            // tc->set_scale({0.01, 0.01, 0.01});

            // LoadManifestInfo manifesto {
            //     .parent = entity,
            //     .path = "assets/models/Sponza/glTF/Sponza.gltf",
            //     .thread_pool = thread_pool,
            //     .asset_processor = asset_processor,
            // };

            LoadManifestInfo manifesto {
                .parent = entity,
                .path = "assets/models/Bistro/Bistro.glb",
                .thread_pool = thread_pool,
                .asset_processor = asset_processor,
            };

            // LoadManifestInfo manifesto {
            //     .parent = entity,
            //     .path = "assets/models/deccer-cubes/SM_Deccer_Cubes_Textured.gltf",
            //     .scene = scene.get(),
            //     .thread_pool = thread_pool,
            //     .asset_processor = asset_processor,
            // };

            asset_manager->load_model(manifesto);
        }

        {
            auto entity = scene->create_entity("test");
            auto* tc = add_transform(entity);
            tc->set_position({0.0, 0.0, 0.0});

            LoadManifestInfo manifesto {
                .parent = entity,
                .path = "assets/models/Sponza/glTF/Sponza.gltf",
                .thread_pool = thread_pool,
                .asset_processor = asset_processor,
            };

            // LoadManifestInfo manifesto {
            //     .parent = entity,
            //     .path = "assets/models/Bistro/Bistro.glb",
            //     .thread_pool = thread_pool,
            //     .asset_processor = asset_processor,
            // };

            asset_manager->load_model(manifesto);
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
                camera.camera.resize(static_cast<i32>(window.get_width()), static_cast<i32>(window.get_height()));
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
                context.device.submit_commands(daxa::CommandSubmitInfo {
                    .command_lists = { {std::move(commands.upload_commands), std::move(cmd_list)} }
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
        camera.update(window, delta_time);

        // auto query_transforms = scene->world->query_builder<GlobalTransformComponent, LocalTransformComponent, GlobalTransformComponent*>().term_at(3).cascade(flecs::ChildOf).optional().build();
        // query_transforms.each([&](flecs::entity entity, GlobalTransformComponent& gtc, LocalTransformComponent& ltc, GlobalTransformComponent* parent_gtc) {
        //     std::cout << entity.name() << std::endl;
        // });

        // scene->world->query<LocalTransformComponent>().each([&](flecs::entity entity, LocalTransformComponent& gtc) {
        //     std::cout << entity.name() << std::endl;
        //     std::cout << gtc.position.x << " " << gtc.position.y << " " << gtc.position.z << std::endl;
        // });

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
        this->context.shader_globals.resolution = { window.get_width(), window.get_height() };

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
            this->context.shader_globals.frustum_planes[i] = *reinterpret_cast<daxa_f32vec4*>(&planes[i]);
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

        ImGui::Begin("Scene Hierarchy");
        // scene->world->query<ModelComponent>().each([&](flecs::entity entity, ModelComponent& gtc) {
        //     std::function<void(flecs::entity, int)> print_hierarchy = [&](flecs::entity e, int indent) {
        //         std::string indent_str = "";
        //         for (int i = 0; i < indent; i++) indent_str += "  ";
        //         ImGui::Text(std::format("{} {}", indent_str, std::string{e.name()}).c_str());

        //         e.children([&](flecs::entity child) {
        //             print_hierarchy(child, indent + 1);
        //         });
        //     };
        //     print_hierarchy(entity, 0);
        // });
        ImGui::End();

        ImGui::Begin("Object Properties");
        ImGui::End();

        renderer.ui_update();
    }

}