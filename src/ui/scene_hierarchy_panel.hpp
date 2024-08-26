#pragma once

#include <pch.hpp>
#include "ecs/entity.hpp"

namespace foundation {
    struct SceneHierarchyPanel {
        explicit SceneHierarchyPanel(Scene* _scene);
        ~SceneHierarchyPanel() = default;

        void draw();

        void set_context(Scene* _scene);

        void tree(Entity& entity, const u32 iteration);

        template<typename T>
        void draw_component(Entity entity, const std::string_view& component_name);

        Entity selected_entity;
        Scene* scene;
    };
}