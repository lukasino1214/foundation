#pragma once

#include "ecs/asset_manager.hpp"
#include "ecs/asset_processor.hpp"
#include "ecs/scene.hpp"
#include "ui/panels/viewport_panel.hpp"
#include "ui/panels/scene_hierarchy_panel.hpp"
#include <pch.hpp>
#include <graphics/window.hpp>
#include <graphics/camera.hpp>
#include <graphics/renderer.hpp>
#include <graphics/context.hpp>
#include <utils/file_watcher.hpp>
#include <scripting/scripting_engine.hpp>

namespace foundation {
    struct Application {
        Application();
        ~Application();

        auto run() -> i32;
        void update();
        void ui_update();

        AppWindow window;
        Context context;
        FileWatcher file_watcher = {};
        ScriptingEngine scripting_engine = {};
        std::shared_ptr<Scene> scene;
        std::unique_ptr<AssetProcessor> asset_processor;
        std::unique_ptr<AssetManager> asset_manager;
        std::unique_ptr<Renderer> renderer;
        ControlledCamera3D camera = {};

        f32 delta_time = 0.016f;
        std::chrono::time_point<std::chrono::steady_clock> last_time_point = {};

        std::unique_ptr<ThreadPool> thread_pool;
        ViewportPanel viewport_panel;
        SceneHierarchyPanel scene_hierarchy_panel;
    };
}