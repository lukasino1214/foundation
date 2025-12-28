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

namespace foundation {
    Scene::Scene(const std::string_view& _name, Context* _context, NativeWIndow* _window)
     : name{_name}, world{std::make_unique<flecs::world>()}, context{_context}, window{_window},
        gpu_entities_data_pool(context, "gpu_entities_data") {
        PROFILE_SCOPE;
        gpu_scene_data = make_task_buffer(context, {
            sizeof(GPUSceneData), 
            daxa::MemoryFlagBits::NONE, 
            "scene data"
        }); 

        query_transforms = world->query_builder<GlobalPosition, GlobalRotation, GlobalScale, GlobalMatrix, LocalPosition, LocalRotation, LocalScale, LocalMatrix, GlobalMatrix*, TransformDirty>().term_at(8).cascade(flecs::ChildOf).optional().cached().build();
    }

    Scene::~Scene() {
        context->device.destroy_buffer(gpu_scene_data.get_state().buffers[0]);
    }

    auto Scene::create_entity(const std::string_view& _name) -> Entity {
        flecs::entity e = world->entity(_name.data());
        e.add<EntityTag>();
        return Entity(e, this);
    }

    void Scene::destroy_entity(Entity& entity) {
        entity.get_handle().destruct();
    }

    void Scene::update(f32 /* delta_time */) {
        PROFILE_SCOPE_NAMED(scene_update);

        std::vector<flecs::entity> queue = {};
        query_transforms.each([&](flecs::entity entity, GlobalPosition& global_position, GlobalRotation& global_rotation, GlobalScale& global_scale, GlobalMatrix& global_matrix, LocalPosition& local_position, LocalRotation& local_rotation, LocalScale& local_scale, LocalMatrix& local_matrix, GlobalMatrix* parent_global_matrix, TransformDirty) {
            entity.children([&](flecs::entity c){
                c.add<TransformDirty>();
            });

            local_matrix.matrix = glm::translate(glm::mat4(1.0f), local_position.position) 
                * glm::toMat4(glm::quat(glm::radians(local_rotation.rotation))) 
                * glm::scale(glm::mat4(1.0f), local_scale.scale);

            global_matrix.matrix = ((parent_global_matrix != nullptr) ? parent_global_matrix->matrix : glm::mat4{1.0f}) * local_matrix.matrix;
            math::decompose_transform(global_matrix.matrix, global_position.position, global_rotation.rotation, global_scale.scale);

            queue.push_back(entity);
        });

        for(auto& entity : queue) {
            if(entity.has<MeshComponent>()) {
                entity.add<GPUTransformDirty>();
            }
            entity.remove<TransformDirty>();
        }
    }

    void Scene::update_gpu(const daxa::TaskInterface& task_interface) {
        PROFILE_SCOPE_NAMED(scene_update_gpu);

        gpu_entities_data_pool.update_buffer(task_interface);

        {
            bool scene_data_updated = false;
            world->each([&](flecs::entity entity, RenderInfo& info, MeshComponent& mesh){
                if(info.is_dirty) {
                    scene_data_updated = true;
                    scene_data.entity_count++;

                    gpu_entities_data_pool.update_handle(task_interface, info.gpu_handle, {
                        .mesh_group_index = mesh.mesh_group_index.value(),
                        // .transform_index = gpu_transform.gpu_handle.index
                    });

                    info.is_dirty = false;
                }

            });

            if(scene_data_updated) {
                auto alloc = task_interface.allocator->allocate_fill(scene_data).value();
                task_interface.recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = task_interface.allocator->buffer(),
                    .dst_buffer = gpu_scene_data.get_state().buffers[0],
                    .src_offset = alloc.buffer_offset,
                    .dst_offset = {},
                    .size = sizeof(GPUSceneData),
                });
            }
        }
    }
}