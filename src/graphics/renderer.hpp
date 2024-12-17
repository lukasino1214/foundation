#pragma once

#include "ecs/asset_manager.hpp"
#include "ecs/scene.hpp"
#include <graphics/window.hpp>
#include <graphics/context.hpp>
#include <daxa/utils/imgui.hpp>

namespace foundation {
    struct Renderer {
        Renderer(AppWindow* _window, Context* _context, Scene* _scene, AssetManager* _asset_manager);
        ~Renderer();

        void render();

        void ui_render_start();
        void ui_update();
        void ui_render_end();

        void window_resized();

        void recreate_framebuffer(const glm::uvec2& size);
        void compile_pipelines();
        void rebuild_task_graph();

        AppWindow* window = {};
        Context* context = {};
        Scene* scene = {};
        AssetManager* asset_manager = {};

        daxa::TaskImage swapchain_image = {};
        daxa::TaskImage render_image = {};
        daxa::TaskImage visibility_image = {};
        daxa::TaskImage depth_image_d32 = {};
        daxa::TaskImage depth_image_u32 = {};
        daxa::TaskImage depth_image_f32 = {};
        daxa::TaskImage overdraw_image = {};

        std::vector<daxa::TaskImage> images = {};
        std::vector<std::pair<daxa::ImageInfo, daxa::TaskImage>> frame_buffer_images = {};

        std::vector<daxa::TaskBuffer> buffers = {};

        daxa::TaskGraph render_task_graph = {};

        daxa::ImGuiRenderer imgui_renderer = {};

        bool startup = true;
        glm::vec2 viewport_size = { 0, 0 };
        i32 gizmo_type = 0;
    };
}