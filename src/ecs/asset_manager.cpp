#include "asset_manager.hpp"
#include "ecs/components.hpp"
#include "graphics/helper.hpp"

#include <fastgltf/types.hpp>

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <math/decompose.hpp>

namespace foundation {
    AssetManager::AssetManager(Context* _context, Scene* _scene) : context{_context}, scene{_scene} {
        gpu_meshes = make_task_buffer(context, {
            sizeof(Mesh), 
            daxa::MemoryFlagBits::DEDICATED_MEMORY, 
            "meshes"
        });

        gpu_materials = make_task_buffer(context, {
            sizeof(Material), 
            daxa::MemoryFlagBits::DEDICATED_MEMORY, 
            "materials"
        });

        gpu_mesh_groups = make_task_buffer(context, {
            sizeof(MeshGroup),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "mesh groups"
        });

        gpu_mesh_indices = make_task_buffer(context, {
            sizeof(u32),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "mesh indices"
        });

        gpu_meshlet_data = make_task_buffer(context, {
            sizeof(MeshletsData),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "meshlet data"
        });

        gpu_culled_meshlet_data = make_task_buffer(context, {
            sizeof(MeshletsData),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "culled meshlet data"
        });

        gpu_meshlet_index_buffer = make_task_buffer(context, {
            sizeof(MeshletsData),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "meshlet index buffer"
        });
    }
    AssetManager::~AssetManager() {
        context->destroy_buffer(gpu_meshes.get_state().buffers[0]);
        context->destroy_buffer(gpu_materials.get_state().buffers[0]);
        context->destroy_buffer(gpu_mesh_groups.get_state().buffers[0]);
        context->destroy_buffer(gpu_mesh_indices.get_state().buffers[0]);
        context->destroy_buffer(gpu_meshlet_data.get_state().buffers[0]);
        context->destroy_buffer(gpu_culled_meshlet_data.get_state().buffers[0]);
        context->destroy_buffer(gpu_meshlet_index_buffer.get_state().buffers[0]);

        for(auto& mesh_manifest : mesh_manifest_entries) {
            if(!mesh_manifest.virtual_geometry_render_info->mesh_buffer.is_empty()) {
                context->destroy_buffer(mesh_manifest.virtual_geometry_render_info->mesh_buffer);
            }
        }

        for(auto& texture_manifest : material_texture_manifest_entries) {
            if(!texture_manifest.image_id.is_empty()) {
                context->destroy_image(texture_manifest.image_id);
            }
        }
    }

    void AssetManager::load_model(LoadManifestInfo& info) {
        if(!std::filesystem::exists(info.path)) {
            throw std::runtime_error("couldnt not find model: " + info.path.string());
        }

        auto* mc = info.parent.add_component<ModelComponent>();
        mc->path = info.path;

        for(const auto& asset_manifest : gltf_asset_manifest_entries) {
            if(info.path == asset_manifest.path) {
                already_loaded_model(info, asset_manifest);
                return;
            }
        }

        u32 const gltf_asset_manifest_index = static_cast<u32>(gltf_asset_manifest_entries.size());
        u32 const texture_manifest_offset = static_cast<u32>(material_texture_manifest_entries.size());
        u32 const material_manifest_offset = static_cast<u32>(material_manifest_entries.size());
        u32 const mesh_manifest_offset = static_cast<u32>(mesh_manifest_entries.size());
        u32 const mesh_group_manifest_offset = static_cast<u32>(mesh_group_manifest_entries.size());

        {
            fastgltf::Parser parser{};
            fastgltf::Options gltf_options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

            fastgltf::GltfDataBuffer data;
            data.loadFromFile(info.path);
            fastgltf::GltfType type = fastgltf::determineGltfFileType(&data);
            fastgltf::Asset asset;
            if (type == fastgltf::GltfType::glTF) {
                asset = std::move(parser.loadGLTF(&data, info.path.parent_path(), gltf_options).get());
            } else if (type == fastgltf::GltfType::GLB) {
                asset = std::move(parser.loadBinaryGLTF(&data, info.path.parent_path(), gltf_options).get());
            } else {
                
            }

            gltf_asset_manifest_entries.push_back(GltfAssetManifestEntry {
                .path = info.path,
                .gltf_asset = std::make_unique<fastgltf::Asset>(std::move(asset)),
                .texture_manifest_offset = texture_manifest_offset,
                .material_manifest_offset = material_manifest_offset,
                .mesh_manifest_offset = mesh_manifest_offset,
                .mesh_group_manifest_offset = mesh_group_manifest_offset,
                .parent = info.parent
            });
        }

        const auto& asset_manifest = gltf_asset_manifest_entries.back();
        const std::unique_ptr<fastgltf::Asset>& asset = asset_manifest.gltf_asset;

        for (u32 i = 0; i < static_cast<u32>(asset->images.size()); ++i) {
            material_texture_manifest_entries.push_back(TextureManifestEntry{
                .gltf_asset_manifest_index = gltf_asset_manifest_index,
                .asset_local_index = i,
                .material_manifest_indices = {}, 
                .image_id = {},
                .sampler_id = {}, 
                .name = asset->images[i].name.c_str(),
            });
        }

        auto gltf_texture_to_manifest_texture_index = [&](u32 const texture_index) -> std::optional<u32> {
            const bool gltf_texture_has_image_index = asset->textures.at(texture_index).imageIndex.has_value();
            if (!gltf_texture_has_image_index) { return std::nullopt; }
            else { return static_cast<u32>(asset->textures.at(texture_index).imageIndex.value()) + texture_manifest_offset; }
        };

        for (u32 material_index = 0; material_index < static_cast<u32>(asset->materials.size()); material_index++) {
            auto const & material = asset->materials.at(material_index);
            u32 const material_manifest_index = material_manifest_offset + material_index;
            bool const has_normal_texture = material.normalTexture.has_value();
            bool const has_albedo_texture = material.pbrData.baseColorTexture.has_value();
            bool const has_roughness_metalness_texture = material.pbrData.metallicRoughnessTexture.has_value();
            bool const has_emissive_texture = material.emissiveTexture.has_value();
            std::optional<MaterialManifestEntry::TextureInfo> albedo_texture_info = {};
            std::optional<MaterialManifestEntry::TextureInfo> normal_texture_info = {};
            std::optional<MaterialManifestEntry::TextureInfo> roughnes_metalness_info = {};
            std::optional<MaterialManifestEntry::TextureInfo> emissive_info = {};
            if (has_albedo_texture) {
                u32 const texture_index = static_cast<u32>(material.pbrData.baseColorTexture.value().textureIndex);
                auto const has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if (has_index) {
                    albedo_texture_info = MaterialManifestEntry::TextureInfo {
                        .texture_manifest_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0, 
                    };

                    material_texture_manifest_entries.at(albedo_texture_info->texture_manifest_index).material_manifest_indices.push_back({
                        .albedo = true,
                        .material_manifest_index = material_manifest_index,
                    });
                }
            }

            if (has_normal_texture) {
                u32 const texture_index = static_cast<u32>(material.normalTexture.value().textureIndex);
                bool const has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if (has_index) {
                    normal_texture_info = MaterialManifestEntry::TextureInfo {
                        .texture_manifest_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0, 
                    };

                    material_texture_manifest_entries.at(normal_texture_info->texture_manifest_index).material_manifest_indices.push_back({
                        .normal = true,
                        .material_manifest_index = material_manifest_index,
                    });
                }
            }

            if (has_roughness_metalness_texture) {
                u32 const texture_index = static_cast<u32>(material.pbrData.metallicRoughnessTexture.value().textureIndex);
                bool const has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if (has_index) {
                    roughnes_metalness_info = MaterialManifestEntry::TextureInfo {
                        .texture_manifest_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0,
                    };

                    material_texture_manifest_entries.at(roughnes_metalness_info->texture_manifest_index).material_manifest_indices.push_back({
                        .roughness_metalness = true,
                        .material_manifest_index = material_manifest_index,
                    });
                }
            }
            if (has_emissive_texture) {
                u32 const texture_index = static_cast<u32>(material.emissiveTexture.value().textureIndex);
                bool const has_index = gltf_texture_to_manifest_texture_index(texture_index).has_value();
                if (has_index) {
                    emissive_info = MaterialManifestEntry::TextureInfo {
                        .texture_manifest_index = gltf_texture_to_manifest_texture_index(texture_index).value(),
                        .sampler_index = 0,
                    };

                    material_texture_manifest_entries.at(emissive_info->texture_manifest_index).material_manifest_indices.push_back({
                        .emissive = true,
                        .material_manifest_index = material_manifest_index,
                    });
                }
            }
            material_manifest_entries.push_back(MaterialManifestEntry{
                .albedo_info = albedo_texture_info,
                .normal_info = normal_texture_info,
                .roughness_metalness_info = roughnes_metalness_info,
                .emissive_info = emissive_info,
                .metallic_factor = material.pbrData.metallicFactor,
                .roughness_factor = material.pbrData.roughnessFactor,
                .emissive_factor = { material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2] },
                .alpha_mode = s_cast<u32>(material.alphaMode),
                .alpha_cutoff = material.alphaCutoff,
                .double_sided = material.doubleSided,
                .gltf_asset_manifest_index = gltf_asset_manifest_index,
                .asset_local_index = material_index,
                .material_manifest_index = material_manifest_offset + material_index,
                .name = material.name.c_str(),
            });
        }

        struct LoadMeshTask : Task {
            struct TaskInfo {
                LoadMeshInfo load_info = {};
                AssetProcessor * asset_processor = {};
                u32 manifest_index = {};
            };

            TaskInfo info = {};
            LoadMeshTask(const TaskInfo& info) : info{info} { chunk_count = 1; }

            virtual void callback(u32, u32) override {
                info.asset_processor->load_mesh(info.load_info);
            };
        };

        for(u32 mesh_index = 0; mesh_index < asset->meshes.size(); mesh_index++) {
            const auto& gltf_mesh = asset->meshes.at(mesh_index);

            u32 mesh_manifest_indices_offset = s_cast<u32>(mesh_manifest_entries.size());
            mesh_manifest_entries.reserve(mesh_manifest_entries.size() + gltf_mesh.primitives.size());
            
            for(u32 primitive_index = 0; primitive_index < gltf_mesh.primitives.size(); primitive_index++) {
                dirty_meshes.push_back(s_cast<u32>(mesh_manifest_entries.size()));
                mesh_manifest_entries.push_back(MeshManifestEntry {
                    .gltf_asset_manifest_index = gltf_asset_manifest_index,
                    .asset_local_mesh_index = mesh_index,
                    .asset_local_primitive_index = primitive_index,
                    .virtual_geometry_render_info = std::nullopt
                });
            }

            dirty_mesh_groups.push_back(s_cast<u32>(mesh_group_manifest_entries.size()));
            mesh_group_manifest_entries.push_back(MeshGroupManifestEntry {
                .mesh_manifest_indices_offset = mesh_manifest_indices_offset,
                .mesh_count = s_cast<u32>(gltf_mesh.primitives.size()),
                .gltf_asset_manifest_index = gltf_asset_manifest_index,
                .asset_local_index = mesh_index,
                .name = gltf_mesh.name.c_str(),
            });
        }

        std::vector<Entity> node_index_to_entity = {};
        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            node_index_to_entity.push_back(scene->create_entity("gltf asset " + std::to_string(gltf_asset_manifest_entries.size()) + " " + std::string{asset->nodes[node_index].name}));
        }

        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            const auto& node = asset->nodes[node_index];
            Entity& parent_entity = node_index_to_entity[node_index];
            if(node.meshIndex.has_value()) {
                parent_entity.add_component<GlobalTransformComponent>();
                parent_entity.add_component<LocalTransformComponent>();
                auto* mesh_component = parent_entity.add_component<MeshComponent>();
                mesh_component->mesh_group_index = asset_manifest.mesh_group_manifest_offset + node.meshIndex.value();
                parent_entity.add_component<RenderInfo>();
            }

            if(const auto* trs = std::get_if<fastgltf::Node::TRS>(&node.transform)) {
                glm::quat quat;
                quat.x = trs->rotation[0];
                quat.y = trs->rotation[1];
                quat.z = trs->rotation[2];
                quat.w = trs->rotation[3];
                
                glm::mat4 mat = glm::translate(glm::mat4(1.0f), { trs->translation[0], trs->translation[1], trs->translation[2]}) 
                    * glm::toMat4(quat) 
                    * glm::scale(glm::mat4(1.0f), { trs->scale[0], trs->scale[1], trs->scale[2]});
                
                glm::vec3 position, rotation, scale;
                math::decompose_transform(mat, position, rotation, scale);

                auto* local_transform = parent_entity.get_component<LocalTransformComponent>();
                local_transform->set_position(position);
                local_transform->set_rotation(rotation);
                local_transform->set_scale(scale);
            } else if(const auto* transform_matrix = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform)) {
                const glm::mat4 mat = *r_cast<const glm::mat4*>(transform_matrix);
                glm::vec3 position, rotation, scale;
                math::decompose_transform(mat, position, rotation, scale);

                auto* local_transform = parent_entity.get_component<LocalTransformComponent>();
                local_transform->set_position(position);
                local_transform->set_rotation(rotation);
                local_transform->set_scale(scale);
            }

            for(u32 children_index = 0; children_index < node.children.size(); children_index++) {
                Entity& child_entity = node_index_to_entity[children_index];
                child_entity.handle.child_of(parent_entity.handle);
            }
        }

        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            Entity& entity = node_index_to_entity[node_index];
            auto parent = entity.handle.parent();
            if(!parent.null()) {
                entity.handle.child_of(info.parent.handle);
            }
        }

        for(u32 mesh_index = 0; mesh_index < asset->meshes.size(); mesh_index++) {
            const auto& gltf_mesh = asset->meshes.at(mesh_index);
            const auto& mesh_group_manifest = mesh_group_manifest_entries[mesh_group_manifest_offset + mesh_index];
            for(u32 primitive_index = 0; primitive_index < gltf_mesh.primitives.size(); primitive_index++) {
                info.thread_pool->async_dispatch(std::make_shared<LoadMeshTask>(LoadMeshTask::TaskInfo{
                    .load_info = {
                        .asset_path = info.path,
                        .asset = asset.get(),
                        .gltf_mesh_index = mesh_index,
                        .gltf_primitive_index = primitive_index,
                        .material_manifest_offset = material_manifest_offset,
                        .manifest_index = mesh_group_manifest.mesh_manifest_indices_offset + primitive_index,
                    },
                    .asset_processor = info.asset_processor.get(),
                }), TaskPriority::LOW);
            }
        }

        struct LoadTextureTask : Task {
            struct TaskInfo {
                LoadTextureInfo load_info = {};
                AssetProcessor * asset_processor = {};
                u32 manifest_index = {};
            };

            TaskInfo info = {};
            explicit LoadTextureTask(TaskInfo const & info) : info{info} { chunk_count = 1; }

            virtual void callback(u32, u32) override {
                info.asset_processor->load_texture(info.load_info);
            };
        };

        for (u32 gltf_texture_index = 0; gltf_texture_index < static_cast<u32>(asset->images.size()); gltf_texture_index++) {
            auto const texture_manifest_index = gltf_texture_index + asset_manifest.texture_manifest_offset;
            auto const & texture_manifest_entry = material_texture_manifest_entries.at(texture_manifest_index);
            bool used_as_albedo = false;
            for (auto const & material_manifest_index : texture_manifest_entry.material_manifest_indices) {
                used_as_albedo |= material_manifest_index.albedo;
                used_as_albedo |= material_manifest_index.emissive;
            }

            if (!texture_manifest_entry.material_manifest_indices.empty()) {
                info.thread_pool->async_dispatch(
                std::make_shared<LoadTextureTask>(LoadTextureTask::TaskInfo{
                    .load_info = {
                        .asset_path = asset_manifest.path,
                        .asset = asset_manifest.gltf_asset.get(),
                        .gltf_texture_index = gltf_texture_index,
                        .texture_manifest_index = texture_manifest_index,
                        .load_as_srgb = used_as_albedo,
                    },
                    .asset_processor = info.asset_processor.get(),
                }), TaskPriority::LOW);
            }
        }
    }

    void AssetManager::already_loaded_model(LoadManifestInfo& /*info*/, const GltfAssetManifestEntry& /*asset_manifest*/) {
        // const auto& asset = asset_manifest.gltf_asset;
        
        // std::vector<Entity> node_index_to_entity = {};
        // for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
        //     node_index_to_entity.push_back(scene->create_entity("gltf asset " + std::to_string(gltf_asset_manifest_entries.size()) + " " + std::string{asset->nodes[node_index].name}));
        // }

        // for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
        //     const auto& node = asset->nodes[node_index];
        //     Entity& parent_entity = node_index_to_entity[node_index];
        //     parent_entity.add_component<GlobalTransformComponent>();
        //     parent_entity.add_component<LocalTransformComponent>();
        //     auto* mesh_component = parent_entity.add_component<MeshComponent>();
        //     if(node.meshIndex.has_value()) {
        //         mesh_component->mesh_group_index = asset_manifest.mesh_group_manifest_offset + node.meshIndex.value();
        //     } else {
        //         mesh_component->mesh_group_index = std::nullopt;
        //     }

        //     if(const auto* trs = std::get_if<fastgltf::Node::TRS>(&node.transform)) {
        //         glm::quat quat;
        //         quat.x = trs->rotation[0];
        //         quat.y = trs->rotation[1];
        //         quat.z = trs->rotation[2];
        //         quat.w = trs->rotation[3];
                
        //         glm::mat4 mat = glm::translate(glm::mat4(1.0f), { trs->translation[0], trs->translation[1], trs->translation[2]}) 
        //             * glm::toMat4(quat) 
        //             * glm::scale(glm::mat4(1.0f), { trs->scale[0], trs->scale[1], trs->scale[2]});
                
        //         glm::vec3 position, rotation, scale;
        //         math::decompose_transform(mat, position, rotation, scale);

        //         auto* local_transform = parent_entity.get_component<LocalTransformComponent>();
        //         local_transform->set_position(position);
        //         local_transform->set_rotation(rotation);
        //         local_transform->set_scale(scale);
        //     } else if(const auto* trs = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform)) {
        //         const glm::mat4 mat = *r_cast<const glm::mat4*>(trs);
        //         glm::vec3 position, rotation, scale;
        //         math::decompose_transform(mat, position, rotation, scale);

        //         auto* local_transform = parent_entity.get_component<LocalTransformComponent>();
        //         local_transform->set_position(position);
        //         local_transform->set_rotation(rotation);
        //         local_transform->set_scale(scale);
        //     }

        //     for(u32 children_index = 0; children_index < node.children.size(); children_index++) {
        //         Entity& child_entity = node_index_to_entity[children_index];
        //         child_entity.handle.child_of(parent_entity.handle);
        //     }
        // }

        // for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
        //     Entity& entity = node_index_to_entity[node_index];
        //     auto parent = entity.handle.parent();
        //     if(!parent.null()) {
        //         entity.handle.child_of(info.parent.handle);
        //     }
        // }
    }

    auto AssetManager::record_manifest_update(const RecordManifestUpdateInfo& info) -> daxa::ExecutableCommandList {
        ZoneScoped;
        auto cmd_recorder = context->device.create_command_recorder({ .name = "asset manager update" });

        auto realloc = [&](daxa::TaskBuffer& task_buffer, u32 new_size) {
            daxa::BufferId buffer = task_buffer.get_state().buffers[0];
            auto info = context->device.info_buffer(buffer).value();
            if(info.size < new_size) {
                context->destroy_buffer_deferred(cmd_recorder, buffer);
                std::println("INFO: {} resized from {} bytes to {} bytes", std::string{info.name.c_str().data()}, std::to_string(info.size), std::to_string(new_size));
                u32 old_size = s_cast<u32>(info.size);
                info.size = new_size;
                daxa::BufferId new_buffer = context->create_buffer(info);
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = buffer,
                    .dst_buffer = new_buffer,
                    .src_offset = 0,
                    .dst_offset = 0,
                    .size = old_size,
                });

                task_buffer.set_buffers({ .buffers=std::array{new_buffer} });
            }
        };

        auto realloc_special = [&](daxa::TaskBuffer& task_buffer, u32 new_size, auto lambda) {
            daxa::BufferId buffer = task_buffer.get_state().buffers[0];
            auto info = context->device.info_buffer(buffer).value();
            if(info.size < new_size) {
                context->destroy_buffer_deferred(cmd_recorder, buffer);
                std::println("INFO: {} resized from {} bytes to {} bytes", std::string{info.name.c_str().data()}, std::to_string(info.size), std::to_string(new_size));
                info.size = new_size;
                daxa::BufferId new_buffer = context->create_buffer(info);

                auto data = lambda(new_buffer);

                daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                    .size = sizeof(decltype(data)),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = std::string{"staging"} + std::string{info.name.c_str().data()}
                });
                context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
                std::memcpy(context->device.get_host_address(staging_buffer).value(), &data, sizeof(decltype(data)));
                cmd_recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_buffer,
                    .dst_buffer = new_buffer,
                    .src_offset = 0,
                    .dst_offset = 0,
                    .size = sizeof(decltype(data)),
                });

                task_buffer.set_buffers({ .buffers=std::array{new_buffer} });
            }
        };

        for(const auto& mesh_upload_info : info.uploaded_meshes) {
            total_meshlet_count += mesh_upload_info.meshlet_count;
            total_triangle_count += mesh_upload_info.triangle_count;
            total_vertex_count += mesh_upload_info.vertex_count;
        }

        realloc(gpu_meshes, s_cast<u32>(mesh_manifest_entries.size() * sizeof(Mesh)));
        realloc(gpu_materials, s_cast<u32>(material_manifest_entries.size() * sizeof(Material)));
        realloc(gpu_mesh_groups, s_cast<u32>(mesh_group_manifest_entries.size() * sizeof(MeshGroup)));
        realloc(gpu_mesh_indices, s_cast<u32>(mesh_manifest_entries.size() * sizeof(u32)));
        realloc_special(gpu_meshlet_data, total_meshlet_count * sizeof(MeshletData) + sizeof(MeshletsData), [&](const daxa::BufferId& buffer) -> MeshletsData {
            return MeshletsData {
                .count = 0,
                .meshlets = context->device.get_device_address(buffer).value() + sizeof(MeshletsData)
            };
        });
        realloc_special(gpu_culled_meshlet_data, total_meshlet_count * sizeof(MeshletData) + sizeof(MeshletsData), [&](const daxa::BufferId& buffer) -> MeshletsData {
            return MeshletsData {
                .count = 0,
                .meshlets = context->device.get_device_address(buffer).value() + sizeof(MeshletsData)
            };
        });
        realloc_special(gpu_meshlet_index_buffer, total_triangle_count * sizeof(u32) + sizeof(MeshletIndexBuffer), [&](const daxa::BufferId& buffer) -> MeshletIndexBuffer {
            return MeshletIndexBuffer {
                .count = 0,
                .indices = context->device.get_device_address(buffer).value() + sizeof(MeshletIndexBuffer)
            };
        });

        if(!dirty_meshes.empty()) {
            Mesh mesh = {};

            daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer meshes"
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            std::memcpy(context->device.get_host_address(staging_buffer).value(), &mesh, sizeof(Mesh));

            for(const u32 mesh_index : dirty_meshes) {
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_meshes.get_state().buffers[0],
                    .src_offset = 0,
                    .dst_offset = mesh_index * sizeof(Mesh),
                    .size = sizeof(Mesh)
                });
            }
            
            dirty_meshes = {};
        }

        if(!dirty_mesh_groups.empty()) {
            std::vector<MeshGroup> mesh_groups = {};
            std::vector<u32> meshes = {};
            u64 buffer_ptr = context->device.get_device_address(gpu_mesh_indices.get_state().buffers[0]).value();
            mesh_groups.reserve(dirty_mesh_groups.size());
            for(u32 mesh_group_index : dirty_mesh_groups) {
                const auto& mesh_group = mesh_group_manifest_entries[mesh_group_index];
                
                mesh_groups.push_back(MeshGroup {
                    .mesh_indices = buffer_ptr + mesh_group.mesh_manifest_indices_offset * sizeof(u32),
                    .count = mesh_group.mesh_count
                });
                
                meshes.reserve(meshes.size() + mesh_group.mesh_count);
                for(u32 i = 0; i < mesh_group.mesh_count; i++) {
                    meshes.push_back(mesh_group.mesh_manifest_indices_offset + i);
                }
            }

            usize staging_size = mesh_groups.size() * sizeof(MeshGroup) + meshes.size() * sizeof(u32);
            daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = staging_size,
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer scene data"
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            std::byte* ptr = context->device.get_host_address(staging_buffer).value();

            usize offset = 0; 
            std::memcpy(ptr + offset, mesh_groups.data(), mesh_groups.size() * sizeof(MeshGroup));
            offset += mesh_groups.size() * sizeof(MeshGroup);
            std::memcpy(ptr + offset, meshes.data(), meshes.size() * sizeof(u32));
            offset += meshes.size() * sizeof(u32);

            usize mesh_group_offset = 0;
            usize meshes_offset = mesh_group_offset + mesh_groups.size() * sizeof(MeshGroup);
            for(u32 mesh_group_index : dirty_mesh_groups) {
                const auto& mesh_group = mesh_group_manifest_entries[mesh_group_index];

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_mesh_groups.get_state().buffers[0],
                    .src_offset = mesh_group_offset,
                    .dst_offset = mesh_group_index * sizeof(MeshGroup),
                    .size = sizeof(MeshGroup)
                });

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_mesh_indices.get_state().buffers[0],
                    .src_offset = meshes_offset,
                    .dst_offset = mesh_group.mesh_manifest_indices_offset * sizeof(u32),
                    .size = mesh_group.mesh_count * sizeof(u32)
                });

                mesh_group_offset += sizeof(MeshGroup);
                meshes_offset += mesh_group.mesh_count * sizeof(u32);
            }
            dirty_mesh_groups.clear();
        }

        daxa::BufferId material_null_buffer = context->create_buffer({
            .size = static_cast<u32>(sizeof(Material)),
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = "material null buffer",
        });
        context->destroy_buffer_deferred(cmd_recorder, material_null_buffer);
        {
            Material material {
                .albedo_texture_id = {},
                .albedo_sampler_id = {},
                .normal_texture_id = {},
                .normal_sampler_id = {},
                .roughness_metalness_texture_id = {},
                .roughness_metalness_sampler_id = {},
                .emissive_texture_id = {},
                .emissive_sampler_id = {},
                .metallic_factor = 1.0f,
                .roughness_factor = 1.0f,
                .emissive_factor = { 0.0f, 0.0f, 0.0f },
                .alpha_mode = 0,
                .alpha_cutoff = 0.5f,
                .double_sided = 0u
            };

            std::memcpy(context->device.get_host_address_as<Material>(material_null_buffer).value(), &material, sizeof(Material));
        }

        for(auto& mesh_upload_info : info.uploaded_meshes) {
            auto& mesh_manifest = mesh_manifest_entries[mesh_upload_info.manifest_index];
            auto& gltf_asset_manifest = gltf_asset_manifest_entries[mesh_manifest.gltf_asset_manifest_index];
            u32 material_index = mesh_upload_info.material_manifest_offset + static_cast<u32>(gltf_asset_manifest.gltf_asset->meshes[mesh_manifest.asset_local_mesh_index].primitives[mesh_manifest.asset_local_primitive_index].materialIndex.value());
            auto& material_manifest = material_manifest_entries.at(material_index);

            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = material_null_buffer,
                .dst_buffer = gpu_materials.get_state().buffers[0],
                .src_offset = 0,
                .dst_offset = material_manifest.material_manifest_index * sizeof(Material),
                .size = sizeof(Material),
            });

            mesh_manifest.virtual_geometry_render_info = MeshManifestEntry::VirtualGeometryRenderInfo {
                .mesh_buffer = mesh_upload_info.mesh_buffer,
                .material_manifest_index = material_manifest.material_manifest_index,
            };
        }

        if(!info.uploaded_meshes.empty()) {
            daxa::BufferId staging_buffer = context->create_buffer({
                .size = info.uploaded_meshes.size() * sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "mesh manifest upload staging buffer",
            });

            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            Mesh * staging_ptr = context->device.get_host_address_as<Mesh>(staging_buffer).value();

            for (u32 upload_index = 0; upload_index < info.uploaded_meshes.size(); upload_index++) {
                auto const & upload = info.uploaded_meshes[upload_index];
                std::memcpy(staging_ptr + upload_index, &upload.mesh, sizeof(Mesh));

                cmd_recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_meshes.get_state().buffers[0],
                    .src_offset = upload_index * sizeof(Mesh),
                    .dst_offset = upload.manifest_index * sizeof(Mesh),
                    .size = sizeof(Mesh),
                });
            }
        }

        std::vector<u32> dirty_material_manifest_indices = {};
        for(auto& texture_upload_info : info.uploaded_textures) {
            auto& texture_manifest = material_texture_manifest_entries.at(texture_upload_info.manifest_index);
            texture_manifest.image_id = texture_upload_info.dst_image;
            texture_manifest.sampler_id = texture_upload_info.sampler;

            for(auto& material_using_texture_info : texture_manifest.material_manifest_indices) {
                MaterialManifestEntry & material_entry = material_manifest_entries.at(material_using_texture_info.material_manifest_index);
                if (material_using_texture_info.albedo) {
                    material_entry.albedo_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.normal) {
                    material_entry.normal_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.roughness_metalness) {
                    material_entry.roughness_metalness_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.emissive) {
                    material_entry.emissive_info->texture_manifest_index = texture_upload_info.manifest_index;
                }

                if (std::find(dirty_material_manifest_indices.begin(), dirty_material_manifest_indices.end(), material_using_texture_info.material_manifest_index) == dirty_material_manifest_indices.end()) {
                    dirty_material_manifest_indices.push_back(material_using_texture_info.material_manifest_index);
                }
            }
        }

        if(!dirty_material_manifest_indices.empty()) {
            daxa::BufferId staging_buffer = context->create_buffer({
                .size = static_cast<u32>(dirty_material_manifest_indices.size() * sizeof(Material)),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer material manifest",
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            Material* ptr = context->device.get_host_address_as<Material>(staging_buffer).value();
            for (u32 dirty_materials_index = 0; dirty_materials_index < dirty_material_manifest_indices.size(); dirty_materials_index++) {
                MaterialManifestEntry& material = material_manifest_entries.at(dirty_material_manifest_indices.at(dirty_materials_index));
                daxa::ImageId albedo_image_id = {};
                daxa::SamplerId albedo_sampler_id = {};
                daxa::ImageId normal_image_id = {};
                daxa::SamplerId normal_sampler_id = {};
                daxa::ImageId roughness_metalness_image_id = {};
                daxa::SamplerId roughness_metalness_sampler_id = {};
                daxa::ImageId emissive_image_id = {};
                daxa::SamplerId emissive_sampler_id = {};

                if (material.albedo_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.albedo_info.value().texture_manifest_index);
                    albedo_image_id = texture_entry.image_id;
                    albedo_sampler_id = texture_entry.sampler_id;
                }
                if (material.normal_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.normal_info.value().texture_manifest_index);
                    normal_image_id = texture_entry.image_id;
                    normal_sampler_id = texture_entry.sampler_id;
                }
                if (material.roughness_metalness_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.roughness_metalness_info.value().texture_manifest_index);
                    roughness_metalness_image_id = texture_entry.image_id;
                    roughness_metalness_sampler_id = texture_entry.sampler_id;
                }
                if (material.emissive_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.emissive_info.value().texture_manifest_index);
                    emissive_image_id = texture_entry.image_id;
                    emissive_sampler_id = texture_entry.sampler_id;
                }

                ptr[dirty_materials_index] = {
                    .albedo_texture_id = albedo_image_id.default_view(),
                    .albedo_sampler_id = albedo_sampler_id,
                    .normal_texture_id = normal_image_id.default_view(),
                    .normal_sampler_id = normal_sampler_id,
                    .roughness_metalness_texture_id = roughness_metalness_image_id.default_view(),
                    .roughness_metalness_sampler_id = roughness_metalness_sampler_id,
                    .emissive_texture_id = emissive_image_id.default_view(),
                    .emissive_sampler_id = emissive_sampler_id,
                    .metallic_factor = material.metallic_factor,
                    .roughness_factor = material.roughness_factor,
                    .emissive_factor = material.emissive_factor,
                    .alpha_mode = material.alpha_mode,
                    .alpha_cutoff = material.alpha_cutoff,
                    .double_sided = s_cast<b32>(material.double_sided)
                };

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_materials.get_state().buffers[0],
                    .src_offset = sizeof(Material) * dirty_materials_index,
                    .dst_offset = material.material_manifest_index * sizeof(Material),
                    .size = sizeof(Material),
                });
            }
        }

        cmd_recorder.pipeline_barrier({
            .src_access = daxa::AccessConsts::TRANSFER_WRITE,
            .dst_access = daxa::AccessConsts::READ_WRITE,
        });

        return cmd_recorder.complete_current_commands();
    }
}