#pragma once

#include <ui/ui.hpp>
#include "ecs/asset_manager.hpp"
#include "ecs/asset_processor.hpp"
#include "ecs/scene.hpp"
#include "ui/panels/file_browser.hpp"
#include "ui/panels/viewport_panel.hpp"
#include "ui/panels/scene_hierarchy_panel.hpp"
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

        NativeWIndow window;
        Context context;
        std::shared_ptr<Scene> scene;
        std::unique_ptr<AssetProcessor> asset_processor;
        std::unique_ptr<ThreadPool> thread_pool;
        std::unique_ptr<AssetManager> asset_manager;
        std::unique_ptr<Renderer> renderer;
        ControlledCamera3D main_camera = {};
        ControlledCamera3D observer_camera = {};

        f32 delta_time = 0.016f;
        std::chrono::time_point<std::chrono::steady_clock> last_time_point = {};

        ViewportPanel viewport_panel;
        SceneHierarchyPanel scene_hierarchy_panel;
        
        std::vector<std::unique_ptr<UIWindow>> ui_windows = {};
    };
}