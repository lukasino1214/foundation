#pragma once

#include "ecs/scene.hpp"

namespace Shaper {
    struct Entity {
        Entity(flecs::entity _handle, Scene* _scene);
        Entity() = default;

        flecs::entity handle = {};
        Scene* scene = {};

        template<typename T>
        auto add_component() -> T* {
            handle.set<T>(T{});
            return handle.get_mut<T>();
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