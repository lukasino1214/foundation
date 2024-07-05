#include "entity.hpp"
#include "scene.hpp"
#include "components.hpp"

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <math/decompose.hpp>

namespace Shaper {
    Scene::Scene(const std::string_view& _name, Context* _context, AppWindow* _window) : name{_name}, world{std::make_unique<flecs::world>()}, context{_context}, window{_window} {}

    Scene::~Scene() {
        world->query<GlobalTransformComponent>().each([&](GlobalTransformComponent& transform_component){
            if(!transform_component.buffer.is_empty()) { context->device.destroy_buffer(transform_component.buffer); }
        });
    }

    auto Scene::create_entity(const std::string_view& _name) -> Entity {
        return Entity(world->entity(_name.data()), this);
    }

    void Scene::destroy_entity(const Entity& entity) {
        if(entity.handle.has<GlobalTransformComponent>()) {
            auto transform_component = entity.handle.get_ref<GlobalTransformComponent>();
            if(!transform_component->buffer.is_empty()) { context->device.destroy_buffer(transform_component->buffer); }
        }

        entity.handle.destruct();
    }

    void Scene::update(f32 delta_time) {
        ZoneNamedN(scene_update, "scene update", true);
        
        auto query_transforms = world->query_builder<GlobalTransformComponent, LocalTransformComponent, GlobalTransformComponent*>().term_at(3).cascade(flecs::ChildOf).optional().build();
        query_transforms.each([&](flecs::entity entity, GlobalTransformComponent& gtc, LocalTransformComponent& ltc, GlobalTransformComponent* parent_gtc){
            if(gtc.buffer.is_empty()) {
                gtc.buffer = context->device.create_buffer(daxa::BufferInfo{
                    .size = static_cast<u32>(sizeof(TransformInfo)),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE | daxa::MemoryFlagBits::DEDICATED_MEMORY,
                    .name = "transform buffer",
                });
            }

            if(ltc.is_dirty) {
                ltc.model_matrix = glm::translate(glm::mat4(1.0f), ltc.position) 
                    * glm::toMat4(glm::quat(glm::radians(ltc.rotation))) 
                    * glm::scale(glm::mat4(1.0f), ltc.scale);

                ltc.normal_matrix = glm::transpose(glm::inverse(ltc.model_matrix));

                gtc.model_matrix = (parent_gtc ? parent_gtc->model_matrix : glm::mat4{1.0f}) * ltc.model_matrix;
                gtc.normal_matrix = (parent_gtc ? parent_gtc->normal_matrix : glm::mat4{1.0f}) * ltc.normal_matrix;

                math::decompose_transform(gtc.model_matrix, gtc.position, gtc.rotation, gtc.scale);

                TransformInfo* mapped_ptr = reinterpret_cast<TransformInfo*>(context->device.get_host_address(gtc.buffer).value());
                mapped_ptr->model_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&gtc.model_matrix);
                mapped_ptr->normal_matrix = *reinterpret_cast<daxa_f32mat4x4*>(&gtc.normal_matrix);

                ltc.is_dirty = false;
            }
        });
    }
}