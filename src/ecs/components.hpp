#pragma once

namespace foundation {
    struct Entity;
    struct EntityTag {};

    struct LocalPosition { glm::vec3 position{ 0.0f, 0.0f, 0.0f }; };
    struct LocalRotation { glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f }; };
    struct LocalScale { glm::vec3 scale{ 1.0f, 1.0f, 1.0f }; };
    struct LocalMatrix { glm::mat4 matrix{1.0f}; };

    struct GlobalPosition { glm::vec3 position{ 0.0f, 0.0f, 0.0f }; };
    struct GlobalRotation { glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f }; };
    struct GlobalScale { glm::vec3 scale{ 1.0f, 1.0f, 1.0f }; };
    struct GlobalMatrix { glm::mat4 matrix{1.0f}; };

    struct TransformDirty {};
    struct GPUTransformDirty {};
    struct TransformComponent {};

    struct MeshComponentDirty {};
    struct MeshComponent {
        u32 mesh_group_manifest_entry_index = {};

        std::optional<usize> mesh_group_index = {};
    };

    struct PointLightComponentDirty {};
    struct PointLightComponent {
        f32vec3 color;
        f32 intensity;
        f32 range;

        std::optional<usize> gpu_point_light_index = std::nullopt;
    };

    struct SpotLightComponentDirty {};
    struct SpotLightComponent {
        f32vec3 direction;
        f32vec3 color;
        f32 intensity;
        f32 range;
        f32 inner_cone_angle;
        f32 outer_cone_angle;

        std::optional<usize> gpu_spot_light_index = std::nullopt;
    };
}