#pragma once

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
        explicit Scene(const std::string_view& _name);
        ~Scene() = default;

        auto create_entity(const std::string_view& _name) -> Entity;
        void destroy_entity(Entity& entity);

        void update(f32 delta_time);

        std::string name;
        std::unique_ptr<flecs::world> world;
        flecs::query<GlobalPosition, GlobalRotation, GlobalScale, GlobalMatrix, LocalPosition, LocalRotation, LocalScale, LocalMatrix, GlobalMatrix*, TransformDirty> query_transforms = {};
    };
}