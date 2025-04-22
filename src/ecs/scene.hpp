#pragma once

#include "graphics/context.hpp"
#include "graphics/utils/cpu_managed_gpu_pool.hpp"
#include <utils/file_watcher.hpp>
#include <scripting/scripting_engine.hpp>
#include <flecs.h>

namespace foundation {
    struct Entity;

    struct LocalPosition;
    struct LocalRotation;
    struct LocalScale;
    struct LocalMatrix;

    struct GlobalPosition;
    struct GlobalRotation;
    struct GlobalScale;
    struct GlobalMatrix;

    struct TransformDirty;
    struct TransformComponent;
    struct GPUTransformIndex;

    struct Scene {
        Scene(const std::string_view& _name, Context* _context, NativeWIndow* _window, ScriptingEngine* _scripting_engine, FileWatcher* _file_watcher);
        ~Scene();

        auto create_entity(const std::string_view& _name) -> Entity;
        void destroy_entity(Entity& entity);

        void update(f32 delta_time);
        void update_gpu(const daxa::TaskInterface& task_interface);

        std::string name;
        std::unique_ptr<flecs::world> world;
        Context* context;
        NativeWIndow* window;
        ScriptingEngine* scripting_engine = {};
        FileWatcher* file_watcher = {};

        GPUSceneData scene_data = {};
        daxa::TaskBuffer gpu_scene_data = {};
        CPUManagedGPUPool<TransformInfo> gpu_transforms_pool;
        CPUManagedGPUPool<EntityData> gpu_entities_data_pool;
        flecs::query<GlobalPosition, GlobalRotation, GlobalScale, GlobalMatrix, LocalPosition, LocalRotation, LocalScale, LocalMatrix, GlobalMatrix*, TransformDirty, GPUTransformIndex> query_transforms = {};
        std::unordered_map<decltype(CPUManagedGPUPool<TransformInfo>::Handle::index), flecs::entity> transform_handle_to_entity = {};
    };
}