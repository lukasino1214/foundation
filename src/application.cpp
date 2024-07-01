#include <application.hpp>
#include <imgui.h>
#include <glm/gtx/string_cast.hpp>

namespace Shaper {
    Application::Application() : 
        window{1280, 720, "shaper"},
        context{this->window},
        renderer{&window, &context} {

        last_time_point = std::chrono::steady_clock::now();
        camera.camera.resize(static_cast<i32>(window.get_width()), static_cast<i32>(window.get_height()));
    }

    Application::~Application() {

    }

    auto Application::run() -> i32 {
        while(!window.window_state->close_requested) {
            auto new_time_point = std::chrono::steady_clock::now();
            this->delta_time = std::chrono::duration_cast<std::chrono::duration<float, std::chrono::milliseconds::period>>(new_time_point - this->last_time_point).count() * 0.001f;
            this->last_time_point = new_time_point;
            window.update();

            if(window.window_state->resize_requested) {
                renderer.window_resized();
                camera.camera.resize(static_cast<i32>(window.get_width()), static_cast<i32>(window.get_height()));
                window.window_state->resize_requested = false;
            }

            update();
            renderer.render();
        }

        return 0;
    }

    void Application::update() {
        camera.update(window, delta_time);

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
    }

    void Application::ui_update() {
        ImGui::Begin("Performance Statistics");
        ImGui::Text("test");
        f64 total_time = 0.0;
        for(auto& [key, metric] : context.gpu_metrics) {
            total_time += metric->time_elapsed;
            ImGui::Text("%s : %f ms", key.data(), metric->time_elapsed);
        }
        ImGui::End();

        ImGui::Begin("Dock");
        ImGui::End();

        ImGui::Begin("File Browser");
        ImGui::End();

        ImGui::Begin("Scene Hierarchy");
        ImGui::End();

        ImGui::Begin("Object Properties");
        ImGui::End();

        renderer.ui_update();
    }

}