#include "entity.hpp"
#include "components.hpp"

Shaper::Entity::Entity(flecs::entity _handle, Scene* _scene) : handle{_handle}, scene{_scene} {}