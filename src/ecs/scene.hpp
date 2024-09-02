#pragma once

#include "graphics/context.hpp"
#include "graphics/utils/cpu_managed_gpu_pool.hpp"
#include <utils/file_watcher.hpp>
#include <scripting/scripting_engine.hpp>
#include <flecs.h>

namespace foundation {
    struct Entity;

    struct Scene {
        Scene(const std::string_view& _name, Context* _context, AppWindow* _window, ScriptingEngine* _scripting_engine, FileWatcher* _file_watcher);
        ~Scene();

        auto create_entity(const std::string_view& _name) -> Entity;
        void destroy_entity(const Entity& entity);

        void update(f32 delta_time);
        void update_gpu(const daxa::TaskInterface& task_interface);

        std::string name;
        std::unique_ptr<flecs::world> world;
        Context* context;
        AppWindow* window;
        ScriptingEngine* scripting_engine = {};
        FileWatcher* file_watcher = {};

        GPUSceneData scene_data = {};
        daxa::TaskBuffer gpu_scene_data = {};
        CPUManagedGPUPool<TransformInfo> gpu_transforms_pool;
        CPUManagedGPUPool<EntityData> gpu_entities_data_pool;
    };
}