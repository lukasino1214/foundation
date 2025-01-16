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

    auto Entity::has_children() -> bool {
        ecs_iter_t it = ecs_each_pair(*scene->world, EcsChildOf, handle);
        if (ecs_iter_is_true(&it)) {
            return true;
        }

        return false;
    }

    void Entity::set_parent(Entity& entity) {
        handle.child_of(entity.handle);
    }

    auto Entity::get_parent() -> Entity {
        return Entity{handle.parent(), scene};
    }

    auto Entity::has_parent() -> bool {
        return handle.parent().has<EntityTag>();
    }

    void Entity::set_local_position(const glm::vec3& value) {
        handle.set<LocalPosition>({ value });
        handle.add<TransformDirty>();
    }

    void Entity::set_local_rotation(const glm::vec3& value) {
        handle.set<LocalRotation>({ value });
        handle.add<TransformDirty>();
    }

    void Entity::set_local_scale(const glm::vec3& value) {
        handle.set<LocalScale>({ value });
        handle.add<TransformDirty>();
    }

    auto Entity::get_local_position() const -> glm::vec3 {
        return handle.get<LocalPosition>()->position;
    }

    auto Entity::get_local_rotation() const -> glm::vec3 {
        return handle.get<LocalRotation>()->rotation;
    }

    auto Entity::get_local_scale() const -> glm::vec3 {
        return handle.get<LocalScale>()->scale;
    }
    
    auto Entity::get_global_position() const -> glm::vec3 {
        return handle.get<GlobalPosition>()->position;
    }

    auto Entity::get_global_rotation() const -> glm::vec3 {
        return handle.get<GlobalRotation>()->rotation;
    }

    auto Entity::get_global_scale() const -> glm::vec3 {
        return handle.get<GlobalScale>()->scale;
    }
}