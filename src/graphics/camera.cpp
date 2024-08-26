#include "camera.hpp"
#include <glm/gtx/rotate_vector.hpp>

namespace foundation {
    void Camera3D::resize(i32 size_x, i32 size_y) {
        aspect = static_cast<f32>(size_x) / static_cast<f32>(size_y);
        //proj_mat = glm::perspective(glm::radians(fov), aspect, near_clip, far_clip);
        const float f = 1.0f / glm::tan(glm::radians(fov) / 2.0f);
        proj_mat = {
            f / aspect, 0.0f, 0.0f, 0.0f,
            0.0f, f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, near_clip, 0.0f
        };
        proj_mat[1][1] *= -1.0f;
    }

    auto Camera3D::get_vp() -> glm::mat4 {
        return proj_mat * view_mat;
    }

    auto Camera3D::get_view() -> glm::mat4 {
        return view_mat;
    }

    void ControlledCamera3D::update(AppWindow& window, f32 dt) {
        if(window.is_cursor_captured()) {
            rotation.x += static_cast<f32>(window.get_cursor_change_x()) * mouse_sens * 0.0001f * camera.fov;
            rotation.y += static_cast<f32>(window.get_cursor_change_y()) * mouse_sens * 0.0001f * camera.fov;
        }

        constexpr auto MAX_ROT = 1.56825555556f;
        if (rotation.y > MAX_ROT) { rotation.y = MAX_ROT; }
        if (rotation.y < -MAX_ROT) { rotation.y = -MAX_ROT; }

        glm::vec3 forward_direction = glm::normalize(glm::vec3{ glm::cos(rotation.x) * glm::cos(rotation.y), -glm::sin(rotation.y), glm::sin(rotation.x) * glm::cos(rotation.y) });
        glm::vec3 up_direction = glm::vec3{ 0.0f, 1.0f, 0.0f };
        glm::vec3 right_direction = glm::normalize(glm::cross(forward_direction, up_direction));

        glm::vec3 move_direction = glm::vec3{ 0.0f };

        if(window.is_cursor_captured()) { 
            if (window.key_pressed(static_cast<Key>(keybinds.move_pz))) { move_direction += forward_direction; }
            if (window.key_pressed(static_cast<Key>(keybinds.move_nz))) { move_direction -= forward_direction; }
            if (window.key_pressed(static_cast<Key>(keybinds.move_nx))) { move_direction += right_direction; }
            if (window.key_pressed(static_cast<Key>(keybinds.move_px))) { move_direction -= right_direction; }
            if (window.key_pressed(static_cast<Key>(keybinds.move_py))) { move_direction += up_direction; }
            if (window.key_pressed(static_cast<Key>(keybinds.move_ny))) { move_direction -= up_direction; }
        }

        position += move_direction * dt * (window.key_pressed(static_cast<Key>(keybinds.toggle_sprint)) ? sprint_speed : 2.0f) * 7.5f;
        camera.view_mat = glm::lookAt(position, position + forward_direction, glm::vec3{0.0f, 1.0f, 0.0f});
    }
}