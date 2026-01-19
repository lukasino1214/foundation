#include "entity.hpp"
#include "scene.hpp"
#include "components.hpp"

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <math/math.hpp>

namespace foundation {
    Scene::Scene(const std::string_view& _name) : name{_name}, world{std::make_unique<flecs::world>()} {
        query_transforms = world->query_builder<GlobalPosition, GlobalRotation, GlobalScale, GlobalMatrix, LocalPosition, LocalRotation, LocalScale, LocalMatrix, GlobalMatrix*, TransformDirty>().term_at(8).cascade(flecs::ChildOf).optional().cached().build();
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
        PROFILE_SCOPE;

        world->get_world().defer_begin();
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
        world->get_world().defer_end();

        for(auto& entity : queue) {
            if(entity.has<MeshComponent>()) {

            }
            entity.remove<TransformDirty>();
        }
    }
}