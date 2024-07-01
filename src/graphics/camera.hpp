#pragma once

#include <graphics/window.hpp>

namespace Shaper {
    struct Camera3D {
        f32 fov = 60.0f, aspect = 1.0f;
        f32 near_clip = 0.1f, far_clip = 1000.0f;
        glm::mat4 proj_mat{};
        glm::mat4 view_mat{};

        void resize(i32 size_x, i32 size_y);
        auto get_vp() -> glm::mat4;
        auto get_view() -> glm::mat4;
    };

    namespace input {
        struct Keybinds {
            i32 move_pz, move_nz;
            i32 move_px, move_nx;
            i32 move_py, move_ny;
            i32 toggle_pause;
            i32 toggle_sprint;
        };

        static inline constexpr Keybinds DEFAULT_KEYBINDS {
            .move_pz = GLFW_KEY_W,
            .move_nz = GLFW_KEY_S,
            .move_px = GLFW_KEY_A,
            .move_nx = GLFW_KEY_D,
            .move_py = GLFW_KEY_SPACE,
            .move_ny = GLFW_KEY_LEFT_CONTROL,
            .toggle_pause = GLFW_KEY_RIGHT_ALT,
            .toggle_sprint = GLFW_KEY_LEFT_SHIFT,
        };
    }

    struct ControlledCamera3D {
        Camera3D camera{};
        input::Keybinds keybinds = input::DEFAULT_KEYBINDS;
        f32 mouse_sens = 0.1f;
        f32 sprint_speed = 10.0f;

        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};
        glm::vec3 delta_position{ 0.0f };

        f32 drag = 7.5f;
        f32 acceleration = 10.0f;

        void update(AppWindow& window, f32 dt);
    };
}