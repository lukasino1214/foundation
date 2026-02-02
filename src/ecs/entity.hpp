#pragma once

#include "ecs/components.hpp"
#include "ecs/scene.hpp"

namespace foundation {
    struct Entity {
        Entity(flecs::entity _handle, Scene* _scene);
        Entity() = default;

private:
        flecs::entity handle = {};
        Scene* scene = {};
public:
        auto get_handle() -> flecs::entity;

        void set_name(const std::string_view& name);
        auto get_name() -> std::string_view;

        void set_child(Entity& entity);
        auto get_children() -> std::vector<Entity>;
        auto has_children() -> bool;

        void set_parent(Entity& entity);
        auto get_parent() -> Entity;
        auto has_parent() -> bool;

        operator bool() const { return handle.is_valid(); }

        template<typename T>
        auto add_component() -> T* {
            if constexpr (std::is_same_v<T, TransformComponent>) {
                handle.add<GlobalPosition>();
                handle.add<GlobalRotation>();
                handle.add<GlobalScale>();
                handle.add<GlobalMatrix>();
                handle.add<TransformDirty>();

                handle.add<LocalPosition>();
                handle.add<LocalRotation>();
                handle.add<LocalScale>();
                handle.add<LocalMatrix>();
                handle.add<TransformDirty>();
                handle.add<TransformComponent>();
                return nullptr;
            }

            if constexpr(std::is_same_v<T, MeshComponent>) {
                handle.add<GPUMeshDirty>();
            }

            if constexpr(std::is_same_v<T, PointLightComponent>) {
                handle.add<GPULightDirty>();
            }

            if constexpr(std::is_same_v<T, SpotLightComponent>) {
                handle.add<GPULightDirty>();
            }

            handle.set<T>(T{});
            T* component =  handle.get_mut<T>();

            return component;
        }

        template<typename T>
        auto get_component() -> T* {
            return handle.get_mut<T>();
        }

        template<typename T>
        auto has_component() const -> bool {
            return handle.has<T>();
        }

        template<typename T>
        void try_add_component() const {
            if(!has_component<T>()) {
                add_component<T>();
            }
        }

        template<typename T>
        void remove_component() const {
            handle.remove<T>();
        }

        void set_local_position(const glm::vec3& value);
        void set_local_rotation(const glm::quat& value);
        void set_local_scale(const glm::vec3& value);

        [[nodiscard]] auto get_local_position() const -> glm::vec3;
        [[nodiscard]] auto get_local_rotation() const -> glm::quat;
        [[nodiscard]] auto get_local_scale() const -> glm::vec3;

        [[nodiscard]] auto get_global_position() const -> glm::vec3;
        [[nodiscard]] auto get_global_rotation() const -> glm::quat;
        [[nodiscard]] auto get_global_scale() const -> glm::vec3;
    };
}