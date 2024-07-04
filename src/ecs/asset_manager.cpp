#include "asset_manager.hpp"
#include "ecs/components.hpp"

#include <fastgltf/types.hpp>

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace Shaper {
    AssetManager::AssetManager(Context* _context, Scene* _scene) : context{_context}, scene{_scene} {}
    AssetManager::~AssetManager() {
        for(auto& material_manifest : material_manifest_entries) {
            if(!material_manifest.material_buffer.is_empty()) {
                context->device.destroy_buffer(material_manifest.material_buffer);
            }
        }

        for(auto& mesh_manifest : mesh_manifest_entries) {
            if(!mesh_manifest.render_info->vertex_buffer.is_empty()) {
                context->device.destroy_buffer(mesh_manifest.render_info->vertex_buffer);
            }
            if(!mesh_manifest.render_info->index_buffer.is_empty()) {
                context->device.destroy_buffer(mesh_manifest.render_info->index_buffer);
            }
        }

        for(auto& texture_manifest : material_texture_manifest_entries) {
            if(!texture_manifest.image_id.is_empty()) {
                context->device.destroy_image(texture_manifest.image_id);
            }
        }
    }

    void AssetManager::load_model(LoadManifestInfo& info) {
        if(!std::filesystem::exists(info.path)) {
            throw std::runtime_error("couldnt not find model: " + info.path.string());
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
                .gltf_asset_manifest_index = gltf_asset_manifest_index,
                .asset_local_index = material_index,
                .material_buffer = {},
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

            virtual void callback(u32 chunk_index, u32 thread_index) override {
                // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                info.asset_processor->load_mesh(info.load_info);
            };
        };

        for(u32 mesh_index = 0; mesh_index < asset->meshes.size(); mesh_index++) {
            const auto& gltf_mesh = asset->meshes.at(mesh_index);

            u32 mesh_manifest_indices_offset = s_cast<u32>(mesh_manifest_entries.size());
            mesh_manifest_entries.reserve(mesh_manifest_entries.size() + gltf_mesh.primitives.size());
            
            for(u32 primitive_index = 0; primitive_index < gltf_mesh.primitives.size(); primitive_index++) {
                mesh_manifest_entries.push_back(MeshManifestEntry {
                    .gltf_asset_manifest_index = gltf_asset_manifest_index,
                    .asset_local_mesh_index = mesh_index,
                    .asset_local_primitive_index = primitive_index,
                    .render_info = std::nullopt
                });
            }

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
            parent_entity.add_component<GlobalTransformComponent>();
            parent_entity.add_component<LocalTransformComponent>();
            auto* mesh_component = parent_entity.add_component<MeshComponent>();
            if(node.meshIndex.has_value()) {
                mesh_component->mesh_group_index = asset_manifest.mesh_group_manifest_offset + node.meshIndex.value();
            } else {
                mesh_component->mesh_group_index = std::nullopt;
            }

            auto decompose_transform = [](const glm::mat4 &transform, glm::vec3 &translation, glm::vec3 &rotation, glm::vec3 &scale) {
                using namespace glm;
                using T = float;

                mat4 local_matrix(transform);

                if (epsilonEqual(local_matrix[3][3], static_cast<float>(0), epsilon<T>()))
                    return false;

                if (epsilonNotEqual(local_matrix[0][3], static_cast<T>(0), epsilon<T>()) || epsilonNotEqual(local_matrix[1][3], static_cast<T>(0), epsilon<T>()) || epsilonNotEqual(local_matrix[2][3], static_cast<T>(0), epsilon<T>())) {
                    local_matrix[0][3] = local_matrix[1][3] = local_matrix[2][3] = static_cast<T>(0);
                    local_matrix[3][3] = static_cast<T>(1);
                }

                translation = vec3(local_matrix[3]);
                local_matrix[3] = vec4(0, 0, 0, local_matrix[3].w);

                vec3 row[3], Pdum3;

                for (length_t i = 0; i < 3; ++i)
                    for (length_t j = 0; j < 3; ++j)
                        row[i][j] = local_matrix[i][j];

                scale.x = length(row[0]);
                row[0] = detail::scale(row[0], static_cast<T>(1));
                scale.y = length(row[1]);
                row[1] = detail::scale(row[1], static_cast<T>(1));
                scale.z = length(row[2]);
                row[2] = detail::scale(row[2], static_cast<T>(1));

                rotation.y = asin(-row[0][2]);
                if (cos(rotation.y) != 0) {
                    rotation.x = atan2(row[1][2], row[2][2]);
                    rotation.z = atan2(row[0][1], row[0][0]);
                } else {
                    rotation.x = atan2(-row[2][0], row[1][1]);
                    rotation.z = 0;
                }

                rotation = glm::degrees(rotation);

                return true;
            };

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
                decompose_transform(mat, position, rotation, scale);

                auto* local_transform = parent_entity.get_component<LocalTransformComponent>();
                local_transform->set_position(position);
                local_transform->set_rotation(rotation);
                local_transform->set_scale(scale);
            } else if(const auto* trs = std::get_if<fastgltf::Node::TransformMatrix>(&node.transform)) {
                const glm::mat4 mat = *r_cast<const glm::mat4*>(trs);
                glm::vec3 position, rotation, scale;
                decompose_transform(mat, position, rotation, scale);

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
            LoadTextureTask(TaskInfo const & info) : info{info} { chunk_count = 1; }

            virtual void callback(u32 chunk_index, u32 thread_index) override {
                // std::this_thread::sleep_for(std::chrono::milliseconds(100));
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

        auto* mc = info.parent.add_component<ModelComponent>();
        mc->path = info.path;
    }

    auto AssetManager::record_manifest_update(const RecordManifestUpdateInfo& info) -> daxa::ExecutableCommandList {
        ZoneScoped;
        auto cmd_recorder = context->device.create_command_recorder({ .name = "asset manager upload" });

        daxa::BufferId material_null_buffer = context->device.create_buffer({
            .size = static_cast<u32>(sizeof(Material)),
            .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            .name = "material null buffer",
        });
        cmd_recorder.destroy_buffer_deferred(material_null_buffer);
        {
            Material material {
                .albedo_texture_id = {},
                .albedo_sampler_id = {},
                .normal_texture_id = {},
                .normal_sampler_id = {},
                .roughness_metalness_texture_id = {},
                .roughness_metalness_sampler_id = {},
                .emissive_texture_id = {},
                .emissive_sampler_id = {}
            };

            std::memcpy(context->device.get_host_address_as<Material>(material_null_buffer).value(), &material, sizeof(Material));
        }

        for(auto& mesh_upload_info : info.uploaded_meshes) {
            auto& mesh_manifest = mesh_manifest_entries[mesh_upload_info.manifest_index];
            auto& gltf_asset_manifest = gltf_asset_manifest_entries[mesh_manifest.gltf_asset_manifest_index];
            u32 material_index = mesh_upload_info.material_manifest_offset + static_cast<u32>(gltf_asset_manifest.gltf_asset->meshes[mesh_manifest.asset_local_mesh_index].primitives[mesh_manifest.asset_local_primitive_index].materialIndex.value());
            auto& material_manifest = material_manifest_entries.at(material_index);

            if(material_manifest.material_buffer.is_empty()) {
                material_manifest.material_buffer = context->device.create_buffer({
                    .size = static_cast<u32>(sizeof(Material)),
                    .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                    .name = "material buffer",
                });

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = material_null_buffer,
                    .dst_buffer = material_manifest.material_buffer,
                    .src_offset = 0,
                    .dst_offset = 0,
                    .size = sizeof(Material),
                });
            }
            
            mesh_manifest.render_info = MeshManifestEntry::RenderInfo {
                .vertex_buffer = mesh_upload_info.vertex_buffer,
                .index_buffer = mesh_upload_info.index_buffer,
                .material_buffer = material_manifest.material_buffer,
                .vertex_count = mesh_upload_info.vertex_count,
                .index_count = mesh_upload_info.index_count,
            };

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
            daxa::BufferId staging_buffer = context->device.create_buffer({
                .size = static_cast<u32>(dirty_material_manifest_indices.size() * sizeof(Material)),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer",
            });
            cmd_recorder.destroy_buffer_deferred(staging_buffer);
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

                ptr[dirty_materials_index].albedo_texture_id = albedo_image_id.default_view();
                ptr[dirty_materials_index].albedo_sampler_id = albedo_sampler_id;
                ptr[dirty_materials_index].normal_texture_id = normal_image_id.default_view();
                ptr[dirty_materials_index].normal_sampler_id = normal_sampler_id;
                ptr[dirty_materials_index].roughness_metalness_texture_id = roughness_metalness_image_id.default_view();
                ptr[dirty_materials_index].roughness_metalness_sampler_id = roughness_metalness_sampler_id;
                ptr[dirty_materials_index].emissive_texture_id = emissive_image_id.default_view();
                ptr[dirty_materials_index].emissive_sampler_id = emissive_sampler_id;

                if(material.material_buffer.is_empty()) {
                    material.material_buffer = context->device.create_buffer({
                        .size = static_cast<u32>(sizeof(Material)),
                        .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                        .name = "material buffer",
                    });
                }

                cmd_recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_buffer,
                    .dst_buffer = material.material_buffer,
                    .src_offset = sizeof(Material) * dirty_materials_index,
                    .dst_offset = 0,
                    .size = sizeof(Material),
                });
            }
        }

        cmd_recorder.pipeline_barrier({
            .src_access = daxa::AccessConsts::TRANSFER_WRITE,
            .dst_access = daxa::AccessConsts::READ_WRITE,
        });

        return std::move(cmd_recorder.complete_current_commands());
    }
}