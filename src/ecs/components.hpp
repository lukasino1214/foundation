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
    struct TransformComponent {};

    struct MeshComponent {
        u32 mesh_group_manifest_entry_index = {};
    };

    struct PointLightComponent {
        f32vec3 color;
        f32 intensity;
        f32 range;
    };

    struct SpotLightComponent {
        f32vec3 direction;
        f32vec3 color;
        f32 intensity;
        f32 range;
        f32 inner_cone_angle;
        f32 outer_cone_angle;
    };
}