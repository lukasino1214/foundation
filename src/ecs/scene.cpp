#include "entity.hpp"
#include "scene.hpp"
#include "components.hpp"
#include "graphics/helper.hpp"

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <math/decompose.hpp>

namespace Shaper {
    Scene::Scene(const std::string_view& _name, Context* _context, AppWindow* _window) : name{_name}, world{std::make_unique<flecs::world>()}, context{_context}, window{_window} {}

    Scene::~Scene() {}

    auto Scene::create_entity(const std::string_view& _name) -> Entity {
        flecs::entity e = world->entity(_name.data());
        e.add<EntityTag>();
        return Entity(e, this);
    }

    void Scene::destroy_entity(const Entity& entity) {
        entity.handle.destruct();
    }

    void Scene::update(f32 delta_time) {
        ZoneNamedN(scene_update, "scene update", true);
        
        auto query_transforms = world->query_builder<GlobalTransformComponent, LocalTransformComponent, GlobalTransformComponent*>().term_at(3).cascade(flecs::ChildOf).optional().build();
        query_transforms.each([&](flecs::entity entity, GlobalTransformComponent& gtc, LocalTransformComponent& ltc, GlobalTransformComponent* parent_gtc){
            ZoneNamedN(update_transform, "update transform", true);

            if(ltc.is_dirty) {
                {
                    ZoneNamedN(mark_children, "mark children", true);
                    entity.children([&](flecs::entity c) {
                        Entity child { c, this };
                        if(child.has_component<LocalTransformComponent>()) {
                            auto* child_ltc = child.get_component<LocalTransformComponent>();
                            child_ltc->is_dirty |= true;
                        }
                   });
                }

                {
                    ZoneNamedN(matrix_mul, "matrix mul", true);
                    ltc.model_matrix = glm::translate(glm::mat4(1.0f), ltc.position) 
                        * glm::toMat4(glm::quat(glm::radians(ltc.rotation))) 
                        * glm::scale(glm::mat4(1.0f), ltc.scale);

                    ltc.normal_matrix = glm::transpose(glm::inverse(ltc.model_matrix));

                    gtc.model_matrix = (parent_gtc ? parent_gtc->model_matrix : glm::mat4{1.0f}) * ltc.model_matrix;
                    gtc.normal_matrix = (parent_gtc ? parent_gtc->normal_matrix : glm::mat4{1.0f}) * ltc.normal_matrix;
                }

                {
                    ZoneNamedN(decompose_transform, "decompose transform", true);
                    math::decompose_transform(gtc.model_matrix, gtc.position, gtc.rotation, gtc.scale);
                    ltc.is_dirty = false;
                    gtc.is_dirty = true;
                }
            }
        });
    }

    void Scene::update_gpu(const daxa::TaskInterface& task_inferface, daxa::TaskBuffer& gpu_transforms) {
        ZoneNamedN(scene_update_gpu, "scene update gpu", true);

        auto query_transforms = world->query_builder<GlobalTransformComponent, MeshComponent>().build();
        query_transforms.each([&](GlobalTransformComponent& gtc, MeshComponent& mesh) {
            if(gtc.is_dirty && mesh.mesh_group_index.has_value()) {
                gtc.is_dirty = false;

                auto alloc = task_inferface.allocator->allocate_fill(TransformInfo { 
                    *r_cast<daxa_f32mat4x4*>(&gtc.model_matrix), 
                    *r_cast<daxa_f32mat4x4*>(&gtc.normal_matrix)
                }).value();
                task_inferface.recorder.copy_buffer_to_buffer({
                    .src_buffer = task_inferface.allocator->buffer(),
                    .dst_buffer = task_inferface.get(gpu_transforms).ids[0],
                    .src_offset = alloc.buffer_offset,
                    .dst_offset = mesh.mesh_group_index.value() * sizeof(TransformInfo),
                    .size = sizeof(TransformInfo),
                });
            }
        });
    }
}