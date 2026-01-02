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
    struct GPUTransformDirty {};
    
    struct ModelComponent {
        std::filesystem::path path;
        
        void draw();
    };
    
    struct MeshComponent {
        std::optional<u32> mesh_group_index;
        std::optional<u32> mesh_group_manifest_entry_index;
        
        void draw();
    };

    struct GPUMeshDirty {};

    struct PointLightComponent {
        f32vec3 position;
        f32vec3 color;
        f32 intensity;
        f32 range;

        std::optional<u32> gpu_index;
    };
    
    struct SpotLightComponent {
        f32vec3 position;
        f32vec3 direction;
        f32vec3 color;
        f32 intensity;
        f32 range;
        f32 inner_cone_angle;
        f32 outer_cone_angle;

        std::optional<u32> gpu_index;
    };
    
    struct GPULightDirty {};

    struct AnimationComponent {
        std::vector<f32> weights = {};
    };
}