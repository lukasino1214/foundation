#include "components.hpp"

#include <ecs/entity.hpp>
#include "ui/ui.hpp"

namespace foundation {
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

        // static std::array<f32, 3> reset_values = { 0.0f, 0.0f, 0.0f };
        // static std::array<const char*, 3> tooltips = { "Some tooltip.", "Some tooltip.", "Some tooltip." };

        // GUI::vec3_property("Extent:", extent, reset_values.data(), tooltips.data());
        // GUI::vec3_property("Color:", color, reset_values.data(), tooltips.data());

        GUI::end_properties();
    }
}