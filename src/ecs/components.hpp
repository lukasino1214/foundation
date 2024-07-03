#pragma once
#include <pch.hpp>
#include <flecs.h>

struct Entity;
struct AppWindow;
struct Physics;

struct LocalTransformComponent {
    glm::vec3 position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
    glm::mat4 model_matrix{1.0f};
    glm::mat4 normal_matrix{1.0f};
    bool is_dirty = true;

    void set_position(const glm::vec3 _position);
    void set_rotation(const glm::vec3 _rotation);
    void set_scale(const glm::vec3 _scale);

    auto get_position() -> glm::vec3;
    auto get_rotation() -> glm::vec3;
    auto get_scale() -> glm::vec3;
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
};

struct ModelComponent {
    std::filesystem::path path;
};

struct MeshComponent {
    daxa::BufferId vertex_buffer = {};
    daxa::BufferId index_buffer = {};
    daxa::BufferId material_buffer = {};

    u32 vertex_count = {};
    u32 index_count = {};
};