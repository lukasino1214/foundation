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
    Scene::Scene(const std::string_view& _name, Context* _context, AppWindow* _window, ScriptingEngine* _scripting_engine, FileWatcher* _file_watcher)
     : name{_name}, world{std::make_unique<flecs::world>()}, context{_context}, window{_window}, scripting_engine{_scripting_engine}, file_watcher{_file_watcher},
        gpu_transforms_pool(context, "gpu_transforms"), gpu_entities_data_pool(context, "gpu_entities_data") {
        PROFILE_SCOPE;
        gpu_scene_data = make_task_buffer(context, {
            sizeof(GPUSceneData), 
            daxa::MemoryFlagBits::DEDICATED_MEMORY, 
            "scene data"
        }); 
        query_transforms = world->query_builder<GlobalTransformComponent, LocalTransformComponent, GlobalTransformComponent*>().term_at(3).cascade(flecs::ChildOf).optional().build();
    }

    Scene::~Scene() {
        context->destroy_buffer(gpu_scene_data.get_state().buffers[0]);
    }

    auto Scene::create_entity(const std::string_view& _name) -> Entity {
        flecs::entity e = world->entity(_name.data());
        e.add<EntityTag>();
        return Entity(e, this);
    }

    void Scene::destroy_entity(const Entity& entity) {
        entity.handle.destruct();
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
    
        query_transforms.each([&](flecs::entity entity, GlobalTransformComponent& gtc, LocalTransformComponent& ltc, GlobalTransformComponent* parent_gtc){
            PROFILE_SCOPE_NAMED(update_transform);

            if(ltc.is_dirty) {
                {
                    PROFILE_SCOPE_NAMED(mark_children);
                    entity.children([&](flecs::entity c) {
                        Entity child { c, this };
                        if(child.has_component<LocalTransformComponent>()) {
                            auto* child_ltc = child.get_component<LocalTransformComponent>();
                            child_ltc->is_dirty |= true;
                        }
                   });
                }

                {
                    PROFILE_SCOPE_NAMED(matrix_mul);
                    ltc.model_matrix = glm::translate(glm::mat4(1.0f), ltc.position) 
                        * glm::toMat4(glm::quat(glm::radians(ltc.rotation))) 
                        * glm::scale(glm::mat4(1.0f), ltc.scale);

                    ltc.normal_matrix = glm::transpose(glm::inverse(ltc.model_matrix));

                    gtc.model_matrix = (parent_gtc ? parent_gtc->model_matrix : glm::mat4{1.0f}) * ltc.model_matrix;
                    gtc.normal_matrix = (parent_gtc ? parent_gtc->normal_matrix : glm::mat4{1.0f}) * ltc.normal_matrix;
                }

                {
                    PROFILE_SCOPE_NAMED(decompose_transform);
                    math::decompose_transform(gtc.model_matrix, gtc.position, gtc.rotation, gtc.scale);
                    ltc.is_dirty = false;
                    gtc.is_dirty = true;
                }
            }
        });
    }

    void Scene::update_gpu(const daxa::TaskInterface& task_interface) {
        PROFILE_SCOPE_NAMED(scene_update_gpu);

        gpu_transforms_pool.update_buffer(task_interface);
        gpu_entities_data_pool.update_buffer(task_interface);

        {
            bool scene_data_updated = false;
            world->each([&](RenderInfo& info, GlobalTransformComponent& tc, MeshComponent&mesh){
                if(info.is_dirty) {
                    scene_data_updated = true;
                    scene_data.entity_count++;

                    gpu_entities_data_pool.update_handle(task_interface, info.gpu_handle, {
                        .mesh_group_index = mesh.mesh_group_index.value(),
                        .transform_index = tc.gpu_handle.index
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

        world->each([&](GlobalTransformComponent& gtc) {
            if(gtc.is_dirty) {
                gtc.is_dirty = false;

                gpu_transforms_pool.update_handle(task_interface, gtc.gpu_handle, { 
                    gtc.model_matrix, 
                    gtc.normal_matrix
                });
            }
        });

        world->each([&](GlobalTransformComponent& tc,AABBComponent& aabb){
            context->debug_draw_context.aabbs.push_back(ShaderDebugAABBDraw{ 
                .color = aabb.color,
                .transform_index = tc.gpu_handle.index
            });
        });
    }
}