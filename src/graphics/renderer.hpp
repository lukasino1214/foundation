#pragma once

#include <daxa/utils/imgui.hpp>
#include <asset/asset_manager.hpp>

namespace foundation {
    struct NativeWindow;
    struct GPUContext;

    struct Renderer {
        Renderer(NativeWindow* _window, GPUContext* _gpu_context);
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

        daxa::TaskImage swapchain_image = {};
        daxa::TaskImage render_image = {};
        daxa::TaskImage depth_image = {};

        daxa::TaskGraph render_task_graph = {};

        ModelData model_data = {};

        std::vector<daxa::TaskImage> images = {};
        std::vector<std::pair<daxa::ImageInfo, daxa::TaskImage>> frame_buffer_images = {};
        std::vector<daxa::TaskBuffer> buffers = {};

        daxa::ImGuiRenderer imgui_renderer = {};
    };
}