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
}