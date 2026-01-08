#pragma once

#include <graphics/window.hpp>
#include <graphics/context.hpp>
#include <daxa/utils/imgui.hpp>

namespace foundation {
    struct Renderer {
        Renderer(NativeWindow* _window, Context* _context);
        ~Renderer();

        void render();

        void ui_render_start();
        void ui_update();
        void ui_render_end();

        void window_resized();

        void recreate_frame_buffer(const glm::uvec2& size);
        void compile_pipelines();
        void rebuild_task_graph();

        NativeWindow* window = {};
        Context* context = {};

        std::atomic<bool> pipelines_compiled = false;

        daxa::TaskImage swapchain_image = {};
        daxa::TaskImage render_image = {};

        std::vector<daxa::TaskImage> images = {};
        std::vector<std::pair<daxa::ImageInfo, daxa::TaskImage>> frame_buffer_images = {};

        std::vector<daxa::TaskBuffer> buffers = {};

        mutable std::mutex task_graph_mutex;
        daxa::TaskGraph render_task_graph = {};

        daxa::ImGuiRenderer imgui_renderer = {};

        bool startup = true;
        glm::vec2 viewport_size = { 0, 0 };
        i32 gizmo_type = 0;
    };
}