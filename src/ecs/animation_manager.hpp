#pragma once

#include "binary_assets.hpp"
#include "entity.hpp"

namespace foundation {
    struct AnimationManager {
        struct AnimationState {
            f32 current_timestamp = {};
            f32 min_timestamp = {};
            f32 max_timestamp = {};
        };

        struct EntityData {
            flecs::entity entity = {};
            std::vector<flecs::entity> node_index_to_entity = {};
            std::vector<AnimationState> animation_states = {};
        };

        struct AnimationData {
            std::vector<EntityData> entity_data = {};
            std::span<const BinaryAnimation> binary_animations = {};
        };

        AnimationManager(Scene* _scene);
        ~AnimationManager();

        void update(f32 delta_time);
        void add_animation(u32 asset_manifest_index, std::span<const BinaryAnimation> binary_animations, EntityData&& entity_data);

        Scene* scene = {};
        ankerl::unordered_dense::map<u32, AnimationData> animation_datas = {};
    };
}