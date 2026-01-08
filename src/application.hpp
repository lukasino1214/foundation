#pragma once

#include <common/thread_pool.hpp>
#include <graphics/window.hpp>
#include <graphics/camera.hpp>
#include <graphics/renderer.hpp>
#include <graphics/context.hpp>

namespace foundation {
    struct Application {
        Application();
        ~Application();

        auto run() -> i32;
        void update();
        void ui_update();

        NativeWindow window;
        Context context;
        std::unique_ptr<ThreadPool> thread_pool;
        std::unique_ptr<Renderer> renderer;
        ControlledCamera3D main_camera = {};
        ControlledCamera3D observer_camera = {};

        f32 delta_time = 0.016f;
        std::chrono::time_point<std::chrono::steady_clock> last_time_point = {};
    };
}