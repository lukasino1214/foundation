#include "asset_manager.hpp"
#include "ecs/components.hpp"

namespace Shaper {
    AssetManager::AssetManager(Context* _context, Scene* _scene) : context{_context}, scene{_scene} {}
    AssetManager::~AssetManager() {}

    void AssetManager::load_model(LoadManifestInfo& info) {
        if(!std::filesystem::exists(info.path)) {
            throw std::runtime_error("couldnt not find model: " + info.path.string());
        }
        
        u32 const gltf_asset_manifest_index = static_cast<u32>(gltf_asset_manifest_entries.size());
        u32 const texture_manifest_offset = static_cast<u32>(material_texture_manifest_entries.size());
        u32 const material_manifest_offset = static_cast<u32>(material_manifest_entries.size());
        u32 const mesh_manifest_offset = static_cast<u32>(mesh_manifest_entries.size());

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
                .parent = info.parent
            });
        }

        std::unique_ptr<fastgltf::Asset>& asset = gltf_asset_manifest_entries.back().gltf_asset;

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
            u32 const material_manifest_index = material_manifest_entries.size();
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
            LoadMeshTask(TaskInfo const & info) : info{info} { chunk_count = 1; }

            virtual void callback(u32 chunk_index, u32 thread_index) override {
                // std::this_thread::sleep_for(std::chrono::milliseconds(100));
                info.asset_processor->load_mesh(info.load_info);
            };
        };

        for(u32 mesh_index = 0; mesh_index < asset->meshes.size(); mesh_index++) {
            for(u32 primitive_index = 0; primitive_index < asset->meshes[mesh_index].primitives.size(); primitive_index++) {
                u32 mesh_manifest_entry_index = mesh_manifest_entries.size();

                mesh_manifest_entries.push_back(MeshManifestEntry {
                    .gltf_asset_manifest_index = gltf_asset_manifest_index,
                    .asset_local_mesh_index = mesh_index,
                    .asset_local_primitive_index = primitive_index,
                    .child = {}
                });

                info.thread_pool->async_dispatch(std::make_shared<LoadMeshTask>(LoadMeshTask::TaskInfo{
                    .load_info = {
                        .asset_path = info.path,
                        .asset = asset.get(),
                        .gltf_mesh_index = mesh_index,
                        .gltf_primitive_index = primitive_index,
                        .material_manifest_offset = material_manifest_offset,
                        .manifest_index = mesh_manifest_entry_index,
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
            auto const & curr_asset = gltf_asset_manifest_entries.back();
            auto const texture_manifest_index = gltf_texture_index + texture_manifest_offset;
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
                            .asset_path = curr_asset.path,
                            .asset = curr_asset.gltf_asset.get(),
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

            auto entity_name = std::string{"mesh - "} + std::to_string(mesh_upload_info.manifest_index);
            mesh_manifest.child = scene->create_entity(entity_name);
            mesh_manifest.child.handle.child_of(gltf_asset_manifest.parent.handle);
            mesh_manifest.child.add_component<GlobalTransformComponent>();
            mesh_manifest.child.add_component<LocalTransformComponent>();
            auto* mc = mesh_manifest.child.add_component<MeshComponent>();

            mc->vertex_buffer = mesh_upload_info.vertex_buffer;
            mc->index_buffer = mesh_upload_info.index_buffer;
            mc->vertex_count = mesh_upload_info.vertex_count;
            mc->index_count = mesh_upload_info.index_count;
            mc->material_buffer = material_manifest_entries.at(material_index).material_buffer;

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

                mc->material_buffer = material_manifest.material_buffer;
            } else {
                mc->material_buffer = material_manifest.material_buffer;
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
            daxa::BufferId staging_buffer = context->device.create_buffer({
                .size = static_cast<u32>(dirty_material_manifest_indices.size() * sizeof(Material)),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer",
            });
            cmd_recorder.destroy_buffer_deferred(staging_buffer);
            Material* ptr = context->device.get_host_address_as<Material>(staging_buffer).value();
            for (u32 dirty_materials_index = 0; dirty_materials_index < dirty_material_manifest_indices.size(); dirty_materials_index++) {
                MaterialManifestEntry const & material = material_manifest_entries.at(dirty_material_manifest_indices.at(dirty_materials_index));
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
                    albedo_image_id = texture_entry.image_id.value_or(daxa::ImageId{});
                    albedo_sampler_id = texture_entry.sampler_id.value_or(daxa::SamplerId{});
                }
                if (material.normal_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.normal_info.value().texture_manifest_index);
                    normal_image_id = texture_entry.image_id.value_or(daxa::ImageId{});
                    normal_sampler_id = texture_entry.sampler_id.value_or(daxa::SamplerId{});
                }
                if (material.roughness_metalness_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.roughness_metalness_info.value().texture_manifest_index);
                    roughness_metalness_image_id = texture_entry.image_id.value_or(daxa::ImageId{});
                    roughness_metalness_sampler_id = texture_entry.sampler_id.value_or(daxa::SamplerId{});
                }
                if (material.emissive_info.has_value()) {
                    auto const & texture_entry = material_texture_manifest_entries.at(material.emissive_info.value().texture_manifest_index);
                    emissive_image_id = texture_entry.image_id.value_or(daxa::ImageId{});
                    emissive_sampler_id = texture_entry.sampler_id.value_or(daxa::SamplerId{});
                }

                ptr[dirty_materials_index].albedo_texture_id = albedo_image_id.default_view();
                ptr[dirty_materials_index].albedo_sampler_id = albedo_sampler_id;
                ptr[dirty_materials_index].normal_texture_id = normal_image_id.default_view();
                ptr[dirty_materials_index].normal_sampler_id = normal_sampler_id;
                ptr[dirty_materials_index].roughness_metalness_texture_id = roughness_metalness_image_id.default_view();
                ptr[dirty_materials_index].roughness_metalness_sampler_id = roughness_metalness_sampler_id;
                ptr[dirty_materials_index].emissive_texture_id = emissive_image_id.default_view();
                ptr[dirty_materials_index].emissive_sampler_id = emissive_sampler_id;

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