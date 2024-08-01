#pragma once

#include "ecs/asset_manager.hpp"
#include "ecs/scene.hpp"
#include "graphics/camera.hpp"
#include <graphics/window.hpp>
#include <graphics/context.hpp>
#include <daxa/utils/imgui.hpp>
#include <ui/scene_hierarchy_panel.hpp>

namespace Shaper {
    struct Renderer {
        enum struct Mode : u32 {
            Traditional,
            VirtualGeomtery,
            PathTracing
        };

        Renderer(AppWindow* _window, Context* _context, Scene* _scene, AssetManager* _asset_manager);
        ~Renderer();

        void render();

        void ui_render_start();
        void ui_update(ControlledCamera3D& camera, f32 delta_time, SceneHierarchyPanel& panel);
        void ui_render_end();

        void switch_mode(Mode mode);
        void build_tradional_task_graph();
        void build_virtual_geometry_task_graph();
        void build_path_tracing_task_graph();

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
        daxa::TaskImage depth_image = {};
        daxa::TaskImage depth_image_2 = {};

        std::vector<daxa::TaskImage> images = {};
        std::vector<std::pair<daxa::ImageInfo, daxa::TaskImage>> frame_buffer_images = {};

        std::vector<daxa::TaskBuffer> buffers = {};

        daxa::TaskGraph render_task_graph = {};

        daxa::ImGuiRenderer imgui_renderer = {};

        Mode rendering_mode = Mode::Traditional;
        bool startup = true;
        glm::vec2 viewport_size = { 0, 0 };
        i32 gizmo_type = 0;
    };
}