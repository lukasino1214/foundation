#include "components.hpp"

#include <ecs/entity.hpp>

void LocalTransformComponent::set_position(const glm::vec3 _position) {
    position = _position;
    is_dirty = true;
}

void LocalTransformComponent::set_rotation(const glm::vec3 _rotation) {
    rotation = _rotation;
    is_dirty = true;
}

void LocalTransformComponent::set_scale(const glm::vec3 _scale) {
    scale = _scale;
    is_dirty = true;
}

auto LocalTransformComponent::get_position() -> glm::vec3 {
    return position;
}

auto LocalTransformComponent::get_rotation() -> glm::vec3 {
    return rotation;
}

auto LocalTransformComponent::get_scale() -> glm::vec3 {
    return scale;
}

auto GlobalTransformComponent::get_position() -> glm::vec3 {
    return position;
}

auto GlobalTransformComponent::get_rotation() -> glm::vec3 {
    return rotation;
}

auto GlobalTransformComponent::get_scale() -> glm::vec3 {
    return scale;
}