#include "scene_hierarchy_panel.hpp"

#include "ecs/components.hpp"
#include "ecs/entity.hpp"

#include "ui/ui.hpp"

#include <iostream>


namespace foundation {
    template<typename T>
    void SceneHierarchyPanel::draw_component(Entity entity, const std::string_view& component_name) {
        const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
        if (entity.has_component<T>()) {
            ImVec2 content_region_available = ImGui::GetContentRegionAvail();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });

            f32 line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

            bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(typeid(T).hash_code() << static_cast<uint32_t>(entity.handle)), treeNodeFlags, "%s", component_name.data());
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
                    GUI::begin_properties();
                    std::string name{entity.handle.name()};
                    bool changed = GUI::string_property("Tag:", name, nullptr, ImGuiInputTextFlags_None);
                    if(changed) { entity.handle.set_name(name.c_str()); }
                    GUI::end_properties();
                } else {
                    T* component = component = entity.get_component<T>();
                    component->draw();
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

    SceneHierarchyPanel::SceneHierarchyPanel(Scene* _scene) : scene{_scene} {}

    void SceneHierarchyPanel::tree(Entity& entity, const u32 iteration) {
        ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;

        bool selected = false;
        if (selected_entity) {
            if(selected_entity.handle == entity.handle) {
                selected = true;
                treeNodeFlags |= ImGuiTreeNodeFlags_Selected;
                ImGui::PushStyleColor(ImGuiCol_Header, { 0.18823529411f, 0.18823529411f, 0.18823529411f, 1.0f });
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, { 0.18823529411f, 0.18823529411f, 0.18823529411f, 1.0f });
            }
        }

        bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>((uint64_t)(uint32_t)entity.handle), treeNodeFlags, "%s", entity.handle.name().c_str());

        if (ImGui::IsItemClicked(ImGuiPopupFlags_MouseButtonLeft) || ImGui::IsItemClicked(ImGuiPopupFlags_MouseButtonRight)) {
            selected_entity = entity;
        }

        bool entity_deleted = false;
        if (ImGui::BeginPopupContextItem()) {
            if(selected_entity) {
                if (ImGui::MenuItem("Create Children Entity")) {
                    Entity created_entity = scene->create_entity("Empty Entity");

                    if(selected_entity) {
                        created_entity.handle.child_of(selected_entity.handle);
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
            entity.handle.children([&](flecs::entity _child) {
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
        ImGui::Begin("Scene Hiearchy");

        auto f = scene->world->query_builder<EntityTag>().build();
        f.each([&](flecs::entity e, EntityTag t){
            if(!e.parent().is_valid()) {
                Entity entity = { e, scene };
                tree(entity, 0);
            }
        });

        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) { selected_entity = {}; }
        if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                Entity created_entity = scene->create_entity("Empty Entity");

                if(selected_entity) {
                    created_entity.handle.child_of(selected_entity.handle);
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

            if(selected_entity.has_component<LocalTransformComponent>()) {
                draw_component<LocalTransformComponent>(selected_entity, "Local Transform Component");
            }

            if(selected_entity.has_component<GlobalTransformComponent>()) {
                draw_component<GlobalTransformComponent>(selected_entity, "Global Transform Component");
            }

            if(selected_entity.has_component<ModelComponent>()) {
                draw_component<ModelComponent>(selected_entity, "Model Component");
            }

            if(selected_entity.has_component<MeshComponent>()) {
                draw_component<MeshComponent>(selected_entity, "Mesh Component");
            }

            if(selected_entity.has_component<AABBComponent>()) {
                draw_component<AABBComponent>(selected_entity, "AABB Component");
            }
            
            if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
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

    void SceneHierarchyPanel::set_context(Scene* _scene) {
        scene = _scene;
    }
}