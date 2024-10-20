#include "components.hpp"

#include <ecs/entity.hpp>
#include "ui/ui.hpp"

namespace foundation {
    void LocalTransformComponent::draw() {
        GUI::begin_properties(ImGuiTableFlags_BordersInnerV);

        static std::array<f32, 3> reset_values = { 0.0f, 0.0f, 0.0f };
        static std::array<const char*, 3> tooltips = { "Some tooltip.", "Some tooltip.", "Some tooltip." };

        if (GUI::vec3_property("Position:", position, reset_values.data(), tooltips.data())) { is_dirty = true; }
        if (GUI::vec3_property("Rotation:", rotation, reset_values.data(), tooltips.data())) { is_dirty = true; }

        reset_values = { 1.0f, 1.0f, 1.0f };

        if (GUI::vec3_property("Scale:", scale, reset_values.data(), tooltips.data())) { is_dirty = true; }

        GUI::end_properties();
    }

    void LocalTransformComponent::set_position(glm::vec3 _position) {
        position = _position;
        is_dirty = true;
    }

    void LocalTransformComponent::set_rotation(glm::vec3 _rotation) {
        rotation = _rotation;
        is_dirty = true;
    }

    void LocalTransformComponent::set_scale(glm::vec3 _scale) {
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

    void GlobalTransformComponent::draw() {
        GUI::begin_properties(ImGuiTableFlags_BordersInnerV);

        static std::array<f32, 3> reset_values = { 0.0f, 0.0f, 0.0f };
        static std::array<const char*, 3> tooltips = { "Some tooltip.", "Some tooltip.", "Some tooltip." };

        glm::vec3 _position = position;
        GUI::vec3_property("Position:", _position, reset_values.data(), tooltips.data());
        glm::vec3 _rotation = rotation;
        GUI::vec3_property("Rotation:", _rotation, reset_values.data(), tooltips.data());

        reset_values = { 1.0f, 1.0f, 1.0f };
        glm::vec3 _scale = scale;
        GUI::vec3_property("Scale:", _scale, reset_values.data(), tooltips.data());

        GUI::end_properties();
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

    void ModelComponent::draw() {
        GUI::begin_properties(ImGuiTableFlags_BordersInnerV);

        GUI::end_properties();
    }

    void MeshComponent::draw() {
        GUI::begin_properties(ImGuiTableFlags_BordersInnerV);

        GUI::end_properties();
    }

    void OOBComponent::draw() {
        GUI::begin_properties(ImGuiTableFlags_BordersInnerV);

        static std::array<f32, 3> reset_values = { 0.0f, 0.0f, 0.0f };
        static std::array<const char*, 3> tooltips = { "Some tooltip.", "Some tooltip.", "Some tooltip." };

        GUI::vec3_property("Extent:", extent, reset_values.data(), tooltips.data());
        GUI::vec3_property("Color:", color, reset_values.data(), tooltips.data());

        GUI::end_properties();
    }
}