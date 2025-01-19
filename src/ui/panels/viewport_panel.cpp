#include "viewport_panel.hpp"

#include <utility>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace foundation {
    ViewportPanel::ViewportPanel(Context* _context, AppWindow* _window, daxa::ImGuiRenderer* _imgui_renderer, ResizeCallback  _resize_callback)
        : context{_context}, window{_window}, imgui_renderer{_imgui_renderer}, resize_callback{std::move(_resize_callback)} {}

    void ViewportPanel::draw(ControlledCamera3D& camera, f32 delta_time, daxa::TaskImage& color_image) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("Viewport");
        ImVec2 size = ImGui::GetContentRegionAvail();
        if(viewport_size != *r_cast<glm::vec2*>(&size)) {
            viewport_size = *r_cast<glm::vec2*>(&size);
            camera.camera.resize(s_cast<i32>(size.x), s_cast<i32>(size.y));
            resize_callback({ size.x, size.y });
        }

        ImTextureID image = imgui_renderer->create_texture_id(daxa::ImGuiImageContext {
            .image_view_id = color_image.get_state().images[0].default_view(),
            .sampler_id = std::bit_cast<daxa::SamplerId>(context->shader_globals.samplers.nearest_clamp)
        });

        ImGui::Image(image, size);

        f32 length = 24.0f;

        ImVec2 pos = ImGui::GetWindowContentRegionMin();

        pos.x += ImGui::GetWindowPos().x;
        pos.y += ImGui::GetWindowPos().y;

        glm::vec2 origin = *r_cast<glm::vec2*>(&pos) + glm::vec2{size.x - length - 8.0f,  length + 8.0f};

        auto project_axis = [&](const glm::vec3& axis) -> ImVec2 {
            return ImVec2(
                origin.x + axis.x * length,
                origin.y + axis.y * length
            );
        };

        auto view_rotation = glm::mat3(camera.camera.view_mat);
        ImVec2 end_x = project_axis(view_rotation * glm::vec3{1.0f, 0.0f, 0.0f});
        ImVec2 end_y = project_axis(view_rotation * glm::vec3{0.0f, -1.0f, 0.0f});
        ImVec2 end_z = project_axis(view_rotation * glm::vec3{0.0f, 0.0f, 1.0f});

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        ImU32 color_x = IM_COL32(255, 0, 0, 255);
        draw_list->AddLine(*r_cast<ImVec2*>(&origin), end_x, color_x, 2.0f);
        draw_list->AddText(end_x, color_x, "X");

        ImU32 color_y = IM_COL32(0, 255, 0, 255);
        draw_list->AddLine(*r_cast<ImVec2*>(&origin), end_y, color_y, 2.0f);
        draw_list->AddText(end_y, color_y, "Y");

        ImU32 color_z = IM_COL32(0, 0, 255, 255);
        draw_list->AddLine(*r_cast<ImVec2*>(&origin), end_z, color_z, 2.0f);
        draw_list->AddText(end_z, color_z, "Z");

        glm::mat4 mod_mat = glm::mat4{1.0};

        if(!ImGuizmo::IsUsing()) {
            if(window->key_pressed(Key::U)) { gizmo_type = -1; }
            if(window->key_pressed(Key::I)) { gizmo_type = ImGuizmo::OPERATION::TRANSLATE; }
            if(window->key_pressed(Key::O)) { gizmo_type = ImGuizmo::OPERATION::ROTATE; }
            if(window->key_pressed(Key::P)) { gizmo_type = ImGuizmo::OPERATION::SCALE; }
        }

        if(selected_entity.get_handle().is_alive()) {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());
            mod_mat = selected_entity.get_component<GlobalMatrix>()->matrix;
            glm::mat4 proj_mat =  glm::perspective(glm::radians(camera.camera.fov), camera.camera.aspect, camera.camera.near_clip, camera.camera.far_clip);

            ImGuizmo::Manipulate(glm::value_ptr(camera.camera.get_view()), glm::value_ptr(proj_mat), s_cast<ImGuizmo::OPERATION>(gizmo_type), ImGuizmo::WORLD, glm::value_ptr(mod_mat), nullptr, nullptr, nullptr, nullptr);
        }

        if(ImGui::IsWindowHovered() && !ImGuizmo::IsUsingAny()) { if(window->button_just_pressed(GLFW_MOUSE_BUTTON_1)) { window->capture_cursor(); } }
        if(window->button_just_released(GLFW_MOUSE_BUTTON_1)) { window->release_cursor(); }
        camera.update(*window, delta_time);

        ImGui::End();

        ImGui::PopStyleVar();

        if(selected_entity.get_handle().is_alive() && ImGuizmo::IsUsingAny()) {
            glm::mat4 local_mat = selected_entity.get_component<LocalMatrix>()->matrix;
            glm::mat4 global_mat = selected_entity.get_component<GlobalMatrix>()->matrix;
            glm::mat4 inverse_local_mat = glm::inverse(local_mat);
            glm::mat4 parent_mat = global_mat * inverse_local_mat;
            glm::mat4 entity_mat = mod_mat;

            glm::vec3 inv_parent_scale = glm::vec3(1.0) / glm::vec3{ glm::length(parent_mat[0]),glm::length(parent_mat[1]), glm::length(parent_mat[2]) };

            glm::mat4 mat_col = glm::inverse(parent_mat) * entity_mat;

            entity_mat[0] *= inv_parent_scale.x;
            entity_mat[1] *= inv_parent_scale.y;
            entity_mat[2] *= inv_parent_scale.z;

            glm::mat3 parent_rotation = glm::mat3(
                parent_mat[0] * inv_parent_scale.x,
                parent_mat[1] * inv_parent_scale.y,
                parent_mat[2] * inv_parent_scale.z
            );

            glm::mat3 entity_rotation = glm::inverse(parent_rotation) * glm::mat3(entity_mat);

            entity_mat[0] = glm::vec4(entity_rotation[0], 0.0);
            entity_mat[1] = glm::vec4(entity_rotation[1], 0.0);
            entity_mat[2] = glm::vec4(entity_rotation[2], 0.0);
            entity_mat[3] = mat_col[3];

            glm::vec3 position = entity_mat[3];
            glm::vec3 scale = { glm::length(entity_mat[0]), glm::length(entity_mat[1]),glm::length(entity_mat[2]) };

            entity_mat[0] /= scale.x;
            entity_mat[1] /= scale.y;
            entity_mat[2] /= scale.z;
            glm::vec3 rotation = glm::eulerAngles(glm::quat_cast(entity_mat));

            selected_entity.set_local_position(position);
            selected_entity.set_local_rotation(glm::degrees(rotation));
            selected_entity.set_local_scale(scale);
        }
    }
}