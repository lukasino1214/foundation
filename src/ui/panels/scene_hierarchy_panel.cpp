#include "scene_hierarchy_panel.hpp"

#include <ecs/components.hpp>
#include <ecs/entity.hpp>

#include <ui/ui.hpp>

namespace foundation {
    template<typename T>
    void SceneHierarchyPanel::draw_component(Entity entity, const std::string_view& component_name) {
        const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
        if (entity.has_component<T>()) {
            ImVec2 content_region_available = ImGui::GetContentRegionAvail();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });

            f32 line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

            bool open = ImGui::TreeNodeEx(r_cast<void*>(typeid(T).hash_code() << s_cast<uint32_t>(entity.get_handle())), treeNodeFlags, "%s", component_name.data());
            ImGui::PopStyleVar();

            ImGui::SameLine(content_region_available.x - line_height * 0.5f);
            if (ImGui::Button("+", ImVec2{ line_height, line_height })) {
                ImGui::OpenPopup("ComponentSettings");
            }

            bool remove_component = false;
            if (ImGui::BeginPopup("ComponentSettings")) {
                if (ImGui::MenuItem("Remove component")) {
                    remove_component = true;
                }

                ImGui::EndPopup();
            }

            if (open) {
                if constexpr(std::is_same_v<T, EntityTag>) {
                    // GUI::begin_properties();
                    // std::string name{entity.get_name()};
                    // bool changed = GUI::string_property("Tag:", name, nullptr, ImGuiInputTextFlags_None);
                    // if(changed) { entity.set_name(name); }
                    // GUI::end_properties();
                } else {
                    if constexpr(std::is_same_v<T, TransformComponent>) {
                        {
                           glm::vec3 position = entity.get_local_position();
                            if(ImGui::DragFloat3("Local Position: ", &position[0])) { entity.set_local_position(position); }
                            glm::vec3 rotation = entity.get_local_rotation();
                            if(ImGui::DragFloat3("Local Rotation: ", &rotation[0])) { entity.set_local_rotation(rotation); }
                            glm::vec3 scale = entity.get_local_scale();
                            if(ImGui::DragFloat3("Local Scale: ", &scale[0])) { entity.set_local_scale(scale); }
                        }
                        {
                           glm::vec3 position = entity.get_global_position();
                            if(ImGui::DragFloat3("Global Position: ", &position[0])) {}
                            glm::vec3 rotation = entity.get_global_rotation();
                            if(ImGui::DragFloat3("Global Rotation: ", &rotation[0])) {}
                            glm::vec3 scale = entity.get_global_scale();
                            if(ImGui::DragFloat3("Global Scale: ", &scale[0])) {}
                        }
                    } else {
                        T* component = component = entity.get_component<T>();
                        component->draw();
                    }
                }
                GUI::indent(0.0f, GImGui->Style.ItemSpacing.y);
                ImGui::TreePop();
            }

            if (remove_component) {
    //                World::RemoveComponent<T>(entity);
            }
        }
    }

    template<typename T>
    inline void add_new_component(Entity entity, const std::string_view& component_name) {
        if(!entity.has_component<T>()) {
            if (ImGui::MenuItem(component_name.data())) {
                entity.add_component<T>();
            
                // if constexpr(std::is_same_v<T, MeshComponent>) {
                //     entity.try_add_component<TransformComponent>();
                // } else if constexpr(std::is_same_v<T, PointLightComponent>) {
                //     entity.try_add_component<TransformComponent>();
                // } else if constexpr(std::is_same_v<T, SpotLightComponent>) {
                //     entity.try_add_component<TransformComponent>();
                // }
            }
        }
    }

    SceneHierarchyPanel::SceneHierarchyPanel(Scene* _scene) : scene{_scene} {
        root_entities_query = scene->world->query_builder<RootEntityTag>().build();
    }

    void SceneHierarchyPanel::tree(Entity& entity, const u32 iteration) {
        ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;

        if(!entity.has_children()) { treeNodeFlags |= ImGuiTreeNodeFlags_Leaf; }

        bool selected = false;
        if (selected_entity) {
            if(selected_entity.get_handle() == entity.get_handle()) {
                selected = true;
                treeNodeFlags |= ImGuiTreeNodeFlags_Selected;
                ImGui::PushStyleColor(ImGuiCol_Header, { 0.18823529411f, 0.18823529411f, 0.18823529411f, 1.0f });
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, { 0.18823529411f, 0.18823529411f, 0.18823529411f, 1.0f });
            }
        }

        bool opened = ImGui::TreeNodeEx(r_cast<void*>((uint64_t)(uint32_t)entity.get_handle()), treeNodeFlags, "%s", entity.get_name().data());

        if (ImGui::IsItemClicked(ImGuiPopupFlags_MouseButtonLeft) || ImGui::IsItemClicked(ImGuiPopupFlags_MouseButtonRight)) {
            selected_entity = entity;
        }

        bool entity_deleted = false;
        if (ImGui::BeginPopupContextItem()) {
            if(selected_entity) {
                if (ImGui::MenuItem("Create Children Entity")) {
                    Entity created_entity = scene->create_entity("Empty Entity");

                    if(selected_entity) {
                        selected_entity.set_child(created_entity);
                    }
                }
            }

            if (ImGui::MenuItem("Delete Entity")) {
                entity_deleted = true;
            }

            ImGui::EndPopup();
        }

        if (selected) {
            ImGui::PopStyleColor(2);
        }

        if (opened) {
            entity.get_handle().children([&](flecs::entity _child) {
                Entity child = { _child, scene };
                tree(child, iteration + 1);
            });

            ImGui::TreePop();
        }

        if (entity_deleted) {
            scene->destroy_entity(entity);
            if (selected_entity == entity) {
                selected_entity = {};
            }
        }


        if(iteration == 0) { ImGui::Separator(); }
    }

    void SceneHierarchyPanel::draw() {
        PROFILE_SCOPE;
        ImGui::Begin("Scene Hiearchy");

        {
            PROFILE_SCOPE_NAMED(scene_world_each);
            root_entities_query.each([&](flecs::entity e, RootEntityTag){
                Entity entity = { e, scene };
                tree(entity, 0);
            });
        }

        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) { selected_entity = {}; }
        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                Entity created_entity = scene->create_entity("Empty Entity");

                if(selected_entity) {
                    selected_entity.set_child(created_entity);
                }
            }

            ImGui::EndPopup();
        }

        ImGui::End();

        ImGui::Begin("Object Properties");

        if(selected_entity) {
            if(selected_entity.has_component<EntityTag>()) {
                draw_component<EntityTag>(selected_entity, "Name Component");
            }

            if(selected_entity.has_component<TransformComponent>()) {
                draw_component<TransformComponent>(selected_entity, "Transform Component");
            }

            if(selected_entity.has_component<ModelComponent>()) {
                draw_component<ModelComponent>(selected_entity, "Model Component");
            }

            if(selected_entity.has_component<MeshComponent>()) {
                draw_component<MeshComponent>(selected_entity, "Mesh Component");
            }

            if(selected_entity.has_component<OOBComponent>()) {
                draw_component<OOBComponent>(selected_entity, "OOB Component");
            }
            
            if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
                // add_new_component<TransformComponent>(selected_entity, "Add Transform Component");
                // add_new_component<MeshComponent>(selected_entity, "Add Mesh Component");

                // if(!(selected_entity.has_component<PointLightComponent>() || selected_entity.has_component<SpotLightComponent>())) {
                //     add_new_component<PointLightComponent>(selected_entity, "Add Point Light Component");
                //     add_new_component<SpotLightComponent>(selected_entity, "Add Spot Light Component");
                // }

                ImGui::EndPopup();
            }
        }

        ImGui::End();
    }
}