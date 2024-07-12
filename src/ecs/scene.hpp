#pragma once

#include "graphics/context.hpp"
#include <flecs.h>

namespace Shaper {
    struct Entity;

    struct Scene {
        Scene(const std::string_view& _name, Context* _context, AppWindow* _window);
        ~Scene();

        auto create_entity(const std::string_view& _name) -> Entity;
        void destroy_entity(const Entity& entity);

        void update(f32 delta_time);
        void update_gpu(const daxa::TaskInterface& task_inferface, daxa::TaskBuffer& gpu_transforms);

        std::string name;
        std::unique_ptr<flecs::world> world;
        Context* context;
        AppWindow* window;
    };
}