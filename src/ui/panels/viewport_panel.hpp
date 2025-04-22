#pragma once

#include <graphics/context.hpp>
#include <graphics/camera.hpp>
#include <daxa/utils/imgui.hpp>
#include <ecs/entity.hpp>

namespace foundation {
    struct ViewportPanel {
        using ResizeCallback = std::function<void(const glm::uvec2& size)>;

        ViewportPanel() = default;
        ViewportPanel(Context* _context, NativeWIndow* _window, daxa::ImGuiRenderer* _imgui_renderer, ResizeCallback _resize_callback);
        ~ViewportPanel() = default;

        void draw(ControlledCamera3D& camera, f32 delta_time, daxa::TaskImage& color_image);

        glm::vec2 viewport_size = { 0, 0 };
        Context* context = {};
        NativeWIndow* window = {};
        daxa::ImGuiRenderer* imgui_renderer = {};
        ResizeCallback resize_callback;
        Entity selected_entity = {};
        i32 gizmo_type = -1;
    };
}