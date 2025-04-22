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
    Scene::Scene(const std::string_view& _name, Context* _context, NativeWIndow* _window, ScriptingEngine* _scripting_engine, FileWatcher* _file_watcher)
     : name{_name}, world{std::make_unique<flecs::world>()}, context{_context}, window{_window}, scripting_engine{_scripting_engine}, file_watcher{_file_watcher},
        gpu_transforms_pool(context, "gpu_transforms"), gpu_entities_data_pool(context, "gpu_entities_data") {
        PROFILE_SCOPE;
        gpu_scene_data = make_task_buffer(context, {
            sizeof(GPUSceneData), 
            daxa::MemoryFlagBits::DEDICATED_MEMORY, 
            "scene data"
        }); 

        query_transforms = world->query_builder<GlobalPosition, GlobalRotation, GlobalScale, GlobalMatrix, LocalPosition, LocalRotation, LocalScale, LocalMatrix, GlobalMatrix*, TransformDirty, GPUTransformIndex>().term_at(8).cascade(flecs::ChildOf).optional().cached().build();
    }

    Scene::~Scene() {
        context->destroy_buffer(gpu_scene_data.get_state().buffers[0]);
    }

    auto Scene::create_entity(const std::string_view& _name) -> Entity {
        flecs::entity e = world->entity(_name.data());
        e.add<EntityTag>();
        return Entity(e, this);
    }

    void Scene::destroy_entity(Entity& entity) {
        entity.get_handle().destruct();
    }

    void Scene::update(f32 delta_time) {
        PROFILE_SCOPE_NAMED(scene_update);

        file_watcher->update([&](const std::filesystem::path& path){
            if(path.extension().string() == ".lua") {
                world->query<ScriptComponent>().each([&](flecs::entity entity, ScriptComponent& script){
                    if(script.path == path) {
                        Entity e(entity, this);
                        scripting_engine->init_script(&script, e, path);
                    }
                });
            }
        });

        world->each([&](ScriptComponent& script){
            if(script.lua) {
                auto& lua = *script.lua;
                lua["update"](delta_time);
            }
        });
    }

    void Scene::update_gpu(const daxa::TaskInterface& task_interface) {
        PROFILE_SCOPE_NAMED(scene_update_gpu);

        gpu_transforms_pool.update_buffer(task_interface);
        gpu_entities_data_pool.update_buffer(task_interface);

        {
            bool scene_data_updated = false;
            world->each([&](flecs::entity entity, RenderInfo& info, GPUTransformIndex& gpu_transform, MeshComponent&mesh){
                if(info.is_dirty) {
                    scene_data_updated = true;
                    scene_data.entity_count++;

                    gpu_entities_data_pool.update_handle(task_interface, info.gpu_handle, {
                        .mesh_group_index = mesh.mesh_group_index.value(),
                        .transform_index = gpu_transform.gpu_handle.index
                    });

                    transform_handle_to_entity[gpu_transform.gpu_handle.index] = entity;

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

        std::vector<flecs::entity> queue = {};
        query_transforms.each([&](flecs::entity entity, GlobalPosition& global_position, GlobalRotation& global_rotation, GlobalScale& global_scale, GlobalMatrix& global_matrix, LocalPosition& local_position, LocalRotation& local_rotation, LocalScale& local_scale, LocalMatrix& local_matrix, GlobalMatrix* parent_global_matrix, TransformDirty, GPUTransformIndex& gpu_transform) {
            entity.children([&](flecs::entity c){
                c.add<TransformDirty>();
            });

            local_matrix.matrix = glm::translate(glm::mat4(1.0f), local_position.position) 
                * glm::toMat4(glm::quat(glm::radians(local_rotation.rotation))) 
                * glm::scale(glm::mat4(1.0f), local_scale.scale);

            global_matrix.matrix = ((parent_global_matrix != nullptr) ? parent_global_matrix->matrix : glm::mat4{1.0f}) * local_matrix.matrix;
            math::decompose_transform(global_matrix.matrix, global_position.position, global_rotation.rotation, global_scale.scale);

            if(gpu_transforms_pool.update_handle(task_interface, gpu_transform.gpu_handle, { 
                .model_matrix = global_matrix.matrix
            })) {
                queue.push_back(entity);
            } else {
                LOG_INFO("hehe");
            }
        });

        for(auto& entity : queue) {
            entity.remove<TransformDirty>();
        }

        world->each([&](GPUTransformIndex& gpu_transform, OOBComponent& oob){
            context->shader_debug_draw_context.entity_oob_draws.draw(ShaderDebugEntityOOBDraw{ 
                .color = oob.color,
                .transform_index = gpu_transform.gpu_handle.index
            });
        });
    }
}