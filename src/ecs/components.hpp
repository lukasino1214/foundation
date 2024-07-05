#pragma once
#include <pch.hpp>
#include <flecs.h>

struct Entity;
struct AppWindow;
struct Physics;

namespace Shaper {
    struct EntityTag {};

    struct LocalTransformComponent {
        glm::vec3 position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
        glm::mat4 model_matrix{1.0f};
        glm::mat4 normal_matrix{1.0f};
        bool is_dirty = true;

        void set_position(glm::vec3 _position);
        void set_rotation(glm::vec3 _rotation);
        void set_scale(glm::vec3 _scale);

        auto get_position() -> glm::vec3;
        auto get_rotation() -> glm::vec3;
        auto get_scale() -> glm::vec3;

        void draw();
    };

    struct GlobalTransformComponent {
        glm::vec3 position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
        glm::mat4 model_matrix{1.0f};
        glm::mat4 normal_matrix{1.0f};

        auto get_position() -> glm::vec3;
        auto get_rotation() -> glm::vec3;
        auto get_scale() -> glm::vec3;

        daxa::BufferId buffer = {};

        void draw();
    };

    struct ModelComponent {
        std::filesystem::path path;

        void draw();
    };

    struct MeshComponent {
        std::optional<u32> mesh_group_index;

        void draw();
    };
}