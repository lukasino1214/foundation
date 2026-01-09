#pragma once

#include <utils/thread_pool.hpp>
#include <graphics/camera.hpp>
#include <graphics/window.hpp>
#include <graphics/gpu_context.hpp>
#include <graphics/renderer.hpp>

namespace foundation {
    struct Application {
        Application();
        ~Application();
    
        auto run() -> i32;
        void update();
    
        ThreadPool thread_pool;
        NativeWindow window;
        GPUContext gpu_context; 
        Renderer renderer;
        ControlledCamera3D main_camera = {};

        std::chrono::time_point<std::chrono::steady_clock> last_time_point = {};
        f64 delta_time = 0.0;
    };
}