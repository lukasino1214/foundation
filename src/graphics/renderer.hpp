#pragma once

#include <graphics/window.hpp>
#include <graphics/context.hpp>
#include <daxa/utils/imgui.hpp>

namespace Shaper {
    struct Renderer {
        enum struct Mode : u32 {
            Traditional,
            VirtualGeomtery,
            PathTracing
        };

        Renderer(AppWindow* _window, Context* _context);
        ~Renderer();

        void render();

        void ui_render_start();
        void ui_update();
        void ui_render_end();

        void switch_mode(Mode mode);
        void build_tradional_task_graph();
        void build_virtual_geometry_task_graph();
        void build_path_tracing_task_graph();

        void window_resized();

        void recreate_framebuffer();
        void compile_pipelines();
        void rebuild_task_graph();

        AppWindow* window = {};
        Context* context = {};

        daxa::TaskImage swapchain_image = {};
        daxa::TaskImage render_image = {};

        std::vector<daxa::TaskImage> images = {};
        std::vector<std::pair<daxa::ImageInfo, daxa::TaskImage>> frame_buffer_images = {};

        std::vector<daxa::TaskBuffer> buffers = {};

        daxa::TaskGraph render_task_graph = {};

        daxa::ImGuiRenderer imgui_renderer = {};

        Mode rendering_mode = Mode::Traditional;
        bool startup = true;
    };
}