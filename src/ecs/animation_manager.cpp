#include "animation_manager.hpp"

#include <glm/gtc/quaternion.hpp>

namespace foundation {
    AnimationManager::AnimationManager(Scene* _scene) : scene{_scene} {}
    AnimationManager::~AnimationManager() {}

    void AnimationManager::update(f32 delta_time) {
        for(auto& [_, animation] : animation_datas) {
            for(auto& entity_data : animation.entity_data) {
                auto& animation_states = entity_data.animation_states;

                for(u32 animation_index = 0; animation_index < animation.binary_animations.size(); animation_index++) {
                    const BinaryAnimation& binary_animation = animation.binary_animations[animation_index];
                    AnimationState& animation_state = animation_states[animation_index];
                    
                    animation_state.current_timestamp += delta_time;
                    f32& current_timestamp = animation_state.current_timestamp;
                    
                    if(current_timestamp <= animation_state.min_timestamp) {
                        current_timestamp = animation_state.min_timestamp;
                    }
    
                    if(current_timestamp >= animation_state.max_timestamp) {
                        current_timestamp = animation_state.min_timestamp;
                    }
                    
                    for(const auto& channel : binary_animation.channels) {
                        const auto& sampler = binary_animation.samplers[channel.sampler];
                        Entity entity(entity_data.node_index_to_entity[channel.node], scene);
                        const f32* f32_values = r_cast<const f32*>(sampler.values.data());
                        const f32vec3* f32vec3_values = r_cast<const f32vec3*>(sampler.values.data());
                        const f32vec4* f32vec4_values = r_cast<const f32vec4*>(sampler.values.data());
    
                        for(u32 i = 0; i < sampler.timestamps.size() - 1; i++) {
                            if ((current_timestamp >= sampler.timestamps[i]) && (current_timestamp <= sampler.timestamps[i + 1])) {
                                switch (sampler.interpolation) {
                                    case BinaryAnimation::InterpolationType::Linear: {
                                        f32 alpha = (current_timestamp - sampler.timestamps[i]) / (sampler.timestamps[i + 1] - sampler.timestamps[i]);
                                        
                                        switch (channel.path) {
                                            case BinaryAnimation::PathType::Position: {
                                                f32vec3 position = glm::mix(f32vec3_values[i], f32vec3_values[i + 1], alpha);
                                                entity.set_local_position(position);
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Rotation: {
                                                f32vec4 value_1 = f32vec4_values[i];
                                                f32vec4 value_2 = f32vec4_values[i + 1];
    
                                                glm::quat quat = glm::normalize(glm::slerp(
                                                    glm::quat(value_1.w, value_1.x, value_1.y, value_1.z), 
                                                    glm::quat(value_2.w, value_2.x, value_2.y, value_2.z), 
                                                    alpha
                                                ));
                                                
                                                entity.set_local_rotation(glm::degrees(glm::eulerAngles(quat)));
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Scale: {
                                                f32vec3 scale = glm::mix(f32vec3_values[i], f32vec3_values[i + 1], alpha);
                                                entity.set_local_scale(scale);
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Weights: {
                                                throw std::runtime_error("something went wrong");
                                                break;
                                            }
                                        }
    
                                        break;
                                    }
                                    case BinaryAnimation::InterpolationType::Step: {
                                        switch (channel.path) {
                                            case BinaryAnimation::PathType::Position: {
                                                entity.set_local_position(f32vec3_values[i]);
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Rotation: {
                                                f32vec4 value_1 = f32vec4_values[i];
                                                glm::quat quat = glm::quat(value_1.w, value_1.x, value_1.y, value_1.z);
                                                entity.set_local_rotation(glm::degrees(glm::eulerAngles(quat)));
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Scale: {
                                                entity.set_local_scale(f32vec3_values[i]);
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Weights: {
                                                throw std::runtime_error("something went wrong");
                                                break;
                                            }
                                        }
    
                                        break;
                                    }
                                    case BinaryAnimation::InterpolationType::CubicSpline: {
                                        throw std::runtime_error("something went wrong");
                                        break;
                                    }
                                }
    
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    void AnimationManager::add_animation(u32 asset_manifest_index, const std::vector<BinaryAnimation>& binary_animations, EntityData&& entity_data) {
        std::vector<AnimationState> animation_states = {};
        animation_states.reserve(binary_animations.size());
        for(const BinaryAnimation& animation : binary_animations) {
            f32 min_timestamp = std::numeric_limits<f32>::max();
            f32 max_timestamp = std::numeric_limits<f32>::lowest();

            for(const auto& sampler : animation.samplers) {
                min_timestamp = glm::min(min_timestamp, sampler.timestamps.front());
                max_timestamp = glm::max(max_timestamp, sampler.timestamps.back());
            }

            animation_states.push_back({
                .current_timestamp = 0.0f,
                .min_timestamp = min_timestamp,
                .max_timestamp = max_timestamp,
            });
        }
        entity_data.animation_states = std::move(animation_states);

        auto [it, inserted] = animation_datas.try_emplace(asset_manifest_index);
        auto& entry = it->second;

        if(inserted) { entry.binary_animations = binary_animations; }
        entry.entity_data.push_back(std::move(entity_data));
    }
}