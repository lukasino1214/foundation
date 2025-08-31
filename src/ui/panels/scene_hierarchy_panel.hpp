#pragma once

#include <ecs/entity.hpp>

namespace foundation {
    struct SceneHierarchyPanel {
        explicit SceneHierarchyPanel(Scene* _scene);

        void draw();

        void tree(Entity& entity, const u32 iteration);

        template<typename T>
        void draw_component(Entity entity, const std::string_view& component_name);

        flecs::query<RootEntityTag> root_entities_query = {};
        Entity selected_entity;
        Scene* scene;
    };
}