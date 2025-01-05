#include "entity.hpp"
#include "components.hpp"

namespace foundation {
    Entity::Entity(flecs::entity _handle, Scene* _scene) : handle{_handle}, scene{_scene} {}

    auto Entity::get_handle() -> flecs::entity {
        return handle;
    }

    void Entity::set_name(const std::string_view& name) {
        handle.set_name(name.data());
    }

    auto Entity::get_name() -> std::string_view {
        const flecs::string_view name = handle.name();
        return std::string_view{name.c_str(), name.size()};
    }

    void Entity::set_child(Entity& entity) {
        entity.handle.child_of(handle);
    }

    auto Entity::get_children() -> std::vector<Entity> {
        std::vector<Entity> children = {};

        handle.children([&](flecs::entity child) {
            children.emplace_back(child, scene);
        });

        return children;
    }

    void Entity::set_parent(Entity& entity) {
        handle.child_of(entity.handle);
    }

    auto Entity::get_parent() -> Entity {
        return Entity{handle.parent(), scene};
    }
}