#pragma once

#include "ecs/components.hpp"
#include "ecs/scene.hpp"

namespace foundation {
    struct Entity {
        Entity(flecs::entity _handle, Scene* _scene);
        Entity() = default;

        flecs::entity handle = {};
        Scene* scene = {};

        operator bool() const { return handle.is_valid(); }

        template<typename T>
        auto add_component() -> T* {
            handle.set<T>(T{});
            T* component =  handle.get_mut<T>();

            if constexpr (std::is_same_v<T, GlobalTransformComponent>) {
                component->gpu_handle = scene->gpu_transforms_pool.allocate_handle();
            }

            if constexpr (std::is_same_v<T, RenderInfo>) {
                component->gpu_handle = scene->gpu_entities_data_pool.allocate_handle();
            }

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
    };
}