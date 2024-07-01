#pragma once

#include <pch.hpp>
#include <graphics/window.hpp>
#include <graphics/camera.hpp>
#include <graphics/renderer.hpp>
#include <graphics/context.hpp>

namespace Shaper {
    struct Application {
        Application();
        ~Application();

        auto run() -> i32;
        void update();
        void ui_update();

        AppWindow window;
        Context context;
        Renderer renderer;
        ControlledCamera3D camera = {};


        f32 delta_time = 0.016f;
        std::chrono::time_point<std::chrono::steady_clock> last_time_point = {};
    };
}