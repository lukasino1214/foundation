#pragma once

#include <daxa/utils/imgui.hpp>

namespace foundation {
    struct NativeWindow;
    struct GPUContext;
    struct GPUScene;
    struct AssetManager;
    struct Scene;

    struct Renderer {
        Renderer(NativeWindow* _window, GPUContext* _gpu_context, GPUScene* _gpu_scene, AssetManager* _asset_manager, Scene* _scene);
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
        GPUContext* gpu_context = {};
        AssetManager* asset_manager = {};
        Scene* scene = {};
        GPUScene* gpu_scene = {};
        
        std::atomic<bool> pipelines_compiled = false;
        
        mutable std::mutex task_graph_mutex;
        daxa::TaskGraph render_task_graph = {};
        
        daxa::TaskImage transmittance = {};
        daxa::TaskImage multiscattering = {};
        daxa::TaskImage sky_ibl_cube = {};
        std::vector<daxa::TaskImage> images = {};

        daxa::TaskImage swapchain_image = {};
        daxa::TaskImage render_image = {};
        daxa::TaskImage visibility_image = {};
        daxa::TaskImage depth_image = {};
        daxa::TaskImage normal_image = {};
        daxa::TaskImage bloom_image = {};
        daxa::TaskImage gtao_image = {};
        std::vector<std::pair<daxa::ImageInfo, daxa::TaskImage>> frame_buffer_images = {};

        daxa::TaskBuffer exposure_state = {};
        std::vector<daxa::TaskBuffer> buffers = {};

        daxa::ImGuiRenderer imgui_renderer = {};
        daxa::TaskGraphDebugUi task_graph_debug_ui = {};
    };
}