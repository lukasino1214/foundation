#pragma once

#include "graphics/context.hpp"
#include "graphics/utils/cpu_managed_gpu_pool.hpp"
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

    struct Scene {
        Scene(const std::string_view& _name, Context* _context, NativeWIndow* _window);
        ~Scene();

        auto create_entity(const std::string_view& _name) -> Entity;
        void destroy_entity(Entity& entity);

        void update(f32 delta_time);
        void update_gpu(const daxa::TaskInterface& task_interface);

        std::string name;
        std::unique_ptr<flecs::world> world;
        Context* context;
        NativeWIndow* window;

        flecs::query<GlobalPosition, GlobalRotation, GlobalScale, GlobalMatrix, LocalPosition, LocalRotation, LocalScale, LocalMatrix, GlobalMatrix*, TransformDirty> query_transforms = {};
    };
}