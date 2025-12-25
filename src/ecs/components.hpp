#pragma once

#include "graphics/utils/cpu_managed_gpu_pool.hpp"
#include <flecs.h>

struct Entity;
struct NativeWIndow;
struct Physics;

namespace foundation {
    struct EntityTag {};
    struct RootEntityTag {};

    struct LocalPosition { glm::vec3 position{ 0.0f, 0.0f, 0.0f }; };
    struct LocalRotation { glm::vec3 rotation{ 0.0f, 0.0f, 0.0f }; };
    struct LocalScale { glm::vec3 scale{ 1.0f, 1.0f, 1.0f }; };
    struct LocalMatrix { glm::mat4 matrix{1.0f}; };

    struct GlobalPosition { glm::vec3 position{ 0.0f, 0.0f, 0.0f }; };
    struct GlobalRotation { glm::vec3 rotation{ 0.0f, 0.0f, 0.0f }; };
    struct GlobalScale { glm::vec3 scale{ 1.0f, 1.0f, 1.0f }; };
    struct GlobalMatrix { glm::mat4 matrix{1.0f}; };

    struct TransformDirty {};
    struct TransformComponent {};

    struct ModelComponent {
        std::filesystem::path path;

        void draw();
    };

    struct MeshComponent {
        std::optional<u32> mesh_group_index;
        std::optional<u32> mesh_group_manifest_entry_index;

        void draw();
    };

    struct RenderInfo {
        CPUManagedGPUPool<EntityData>::Handle gpu_handle = {};
        bool is_dirty = true;
    };
}