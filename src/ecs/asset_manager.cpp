#include "asset_manager.hpp"
#include <ecs/components.hpp>
#include <graphics/helper.hpp>

#include <utils/byte_utils.hpp>
#include <utils/zstd.hpp>
#include <math/decompose.hpp>
#include <utils/file_io.hpp>

namespace foundation {
    AssetManager::AssetManager(Context* _context, Scene* _scene, ThreadPool* _thread_pool, AssetProcessor* _asset_processor) : context{_context}, scene{_scene}, thread_pool{_thread_pool}, asset_processor{_asset_processor} {
        ZoneScoped;
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

        gpu_culled_meshlet_indices = make_task_buffer(context, {
            sizeof(MeshletsData),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "culled meshlet indices"
        });

        gpu_meshlet_index_buffer = make_task_buffer(context, {
            sizeof(MeshletsData),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "meshlet index buffer"
        });

        gpu_readback_material = make_task_buffer(context, {
            sizeof(u32),
            daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            "readback material"
        });
    }
    AssetManager::~AssetManager() {
        context->destroy_buffer(gpu_meshes.get_state().buffers[0]);
        context->destroy_buffer(gpu_materials.get_state().buffers[0]);
        context->destroy_buffer(gpu_mesh_groups.get_state().buffers[0]);
        context->destroy_buffer(gpu_mesh_indices.get_state().buffers[0]);
        context->destroy_buffer(gpu_meshlet_data.get_state().buffers[0]);
        context->destroy_buffer(gpu_culled_meshlet_indices.get_state().buffers[0]);
        context->destroy_buffer(gpu_meshlet_index_buffer.get_state().buffers[0]);
        context->destroy_buffer(gpu_readback_material.get_state().buffers[0]);

        for(auto& mesh_manifest : mesh_manifest_entries) {
            if(!mesh_manifest.virtual_geometry_render_info->mesh_buffer.is_empty()) {
                context->destroy_buffer(mesh_manifest.virtual_geometry_render_info->mesh_buffer);
            }
        }

        for(auto& texture_manifest : texture_manifest_entries) {
            if(!texture_manifest.image_id.is_empty()) {
                context->destroy_image(texture_manifest.image_id);
            }
        }
    }

    void AssetManager::load_model(LoadManifestInfo& info) {
        ZoneNamedN(load_model, "load model", true);
        if(!std::filesystem::exists(info.path)) {
            throw std::runtime_error("couldnt not find model: " + info.path.string());
        }

        auto* mc = info.parent.add_component<ModelComponent>();
        mc->path = info.path;

        u32 const asset_manifest_index = s_cast<u32>(asset_manifest_entries.size());
        u32 const texture_manifest_offset = s_cast<u32>(texture_manifest_entries.size());
        u32 const material_manifest_offset = s_cast<u32>(material_manifest_entries.size());
        u32 const mesh_manifest_offset = s_cast<u32>(mesh_manifest_entries.size());
        u32 const mesh_group_manifest_offset = s_cast<u32>(mesh_group_manifest_entries.size());

        std::filesystem::path binary_path = info.path;
        binary_path.replace_extension("bmodel");


        {
            std::vector<byte> data = read_file_to_bytes(binary_path);
            std::vector<byte> uncompressed_data = zstd_decompress(data);

            ByteReader byte_reader{ uncompressed_data.data(), uncompressed_data.size() };

            BinaryHeader header = {};
            byte_reader.read(header.name);
            byte_reader.read(header.version);

            BinaryAssetInfo asset = {};
            byte_reader.read(asset.textures);
            byte_reader.read(asset.materials);
            byte_reader.read(asset.nodes);
            byte_reader.read(asset.mesh_groups);
            byte_reader.read(asset.meshes);

            asset_manifest_entries.push_back(AssetManifestEntry {
                .path = info.path,
                .texture_manifest_offset = texture_manifest_offset,
                .material_manifest_offset = material_manifest_offset,
                .mesh_manifest_offset = mesh_manifest_offset,
                .mesh_group_manifest_offset = mesh_group_manifest_offset,
                .parent = info.parent,
                .asset = std::make_unique<BinaryAssetInfo>(std::move(asset)),
            });
        }

        const auto& asset_manifest = asset_manifest_entries.back();
        const auto& asset = asset_manifest.asset;

        for (u32 i = 0; i < s_cast<u32>(asset->textures.size()); ++i) {
            std::vector<TextureManifestEntry::MaterialManifestIndex> indices = {};
            const BinaryTexture& texture = asset->textures[i];
            indices.reserve(texture.material_indices.size());
            for(const auto& v : texture.material_indices) {
                indices.push_back({
                    .material_type = v.material_type,
                    .material_manifest_index = v.material_index + material_manifest_offset,
                });
            }

            texture_manifest_entries.push_back(TextureManifestEntry{
                .asset_manifest_index = asset_manifest_index,
                .asset_local_index = i,
                .material_manifest_indices = indices, 
                .image_id = {},
                .sampler_id = {},
                .current_resolution = {},
                .max_resolution = texture.resolution,
                .name = texture.name,
            });
        }

        for (u32 material_index = 0; material_index < s_cast<u32>(asset->materials.size()); material_index++) {
            auto make_texture_info = [texture_manifest_offset](const std::optional<BinaryMaterial::BinaryTextureInfo>& info) -> std::optional<MaterialManifestEntry::TextureInfo> {
                if(!info.has_value()) { return std::nullopt; }
                return std::make_optional(MaterialManifestEntry::TextureInfo {
                    .texture_manifest_index = info->texture_index + texture_manifest_offset,
                    .sampler_index = 0
                });
            };

            const BinaryMaterial& material = asset->materials[material_index];
            material_manifest_entries.push_back(MaterialManifestEntry{
                .albedo_info = make_texture_info(material.albedo_info),
                .alpha_mask_info = make_texture_info(material.alpha_mask_info),
                .normal_info = make_texture_info(material.normal_info),
                .roughness_info = make_texture_info(material.roughness_info),
                .metalness_info = make_texture_info(material.metalness_info),
                .emissive_info = make_texture_info(material.emissive_info),
                .metallic_factor = material.metallic_factor,
                .roughness_factor = material.roughness_factor,
                .emissive_factor = material.emissive_factor,
                .alpha_mode = material.alpha_mode,
                .alpha_cutoff = material.alpha_cutoff,
                .double_sided = material.double_sided,
                .asset_manifest_index = asset_manifest_index,
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
                info.asset_processor->load_gltf_mesh(info.load_info);
            };
        };

        for(u32 mesh_group_index = 0; mesh_group_index < asset->mesh_groups.size(); mesh_group_index++) {
            const auto& mesh_group = asset->mesh_groups.at(mesh_group_index);

            u32 mesh_manifest_indices_offset = s_cast<u32>(mesh_manifest_entries.size());
            mesh_manifest_entries.reserve(mesh_manifest_entries.size() + mesh_group.mesh_count);
            
            for(u32 mesh_index = 0; mesh_index < mesh_group.mesh_count; mesh_index++) {
                dirty_meshes.push_back(s_cast<u32>(mesh_manifest_entries.size()));
                mesh_manifest_entries.push_back(MeshManifestEntry {
                    .asset_manifest_index = asset_manifest_index,
                    .asset_local_mesh_index = mesh_group_index,
                    .asset_local_primitive_index = mesh_index,
                    .virtual_geometry_render_info = std::nullopt
                });
            }

            dirty_mesh_groups.push_back(s_cast<u32>(mesh_group_manifest_entries.size()));
            mesh_group_manifest_entries.push_back(MeshGroupManifestEntry {
                .mesh_manifest_indices_offset = mesh_manifest_indices_offset,
                .mesh_count = mesh_group.mesh_count,
                .asset_manifest_index = asset_manifest_index,
                .asset_local_index = mesh_group_index,
                .name = {},
            });
        }

        std::vector<Entity> node_index_to_entity = {};
        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            node_index_to_entity.push_back(scene->create_entity("gltf asset " + std::to_string(asset_manifest_entries.size()) + " " + std::string{asset->nodes[node_index].name}));
        }

        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            const auto& node = asset->nodes[node_index];
            Entity& parent_entity = node_index_to_entity[node_index];
            if(node.mesh_index.has_value()) {
                parent_entity.add_component<GlobalTransformComponent>();
                parent_entity.add_component<LocalTransformComponent>();
                auto* mesh_component = parent_entity.add_component<MeshComponent>();
                mesh_component->mesh_group_index = asset_manifest.mesh_group_manifest_offset + node.mesh_index.value();
                parent_entity.add_component<RenderInfo>();
            }

            glm::vec3 position = {};
            glm::vec3 rotation = {};
            glm::vec3 scale = {};
            math::decompose_transform(node.transform, position, rotation, scale);

            auto* local_transform = parent_entity.get_component<LocalTransformComponent>();
            local_transform->set_position(position);
            local_transform->set_rotation(rotation);
            local_transform->set_scale(scale);

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

        for(u32 mesh_group_index = 0; mesh_group_index < asset->mesh_groups.size(); mesh_group_index++) {
            const auto& mesh_group = asset->mesh_groups.at(mesh_group_index);
            const auto& mesh_group_manifest = mesh_group_manifest_entries[mesh_group_manifest_offset + mesh_group_index];
            for(u32 mesh_index = 0; mesh_index < mesh_group.mesh_count; mesh_index++) {
                thread_pool->async_dispatch(std::make_shared<LoadMeshTask>(LoadMeshTask::TaskInfo{
                    .load_info = {
                        .asset_path = info.path,
                        .asset = asset.get(),
                        .mesh_group_index = mesh_group_index,
                        .mesh_index = mesh_index,
                        .material_manifest_offset = material_manifest_offset,
                        .manifest_index = mesh_group_manifest.mesh_manifest_indices_offset + mesh_index,
                        .file_path = asset_manifest.path.parent_path() / asset->meshes[mesh_group.mesh_offset + mesh_index].file_path
                    },
                    .asset_processor = asset_processor,
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

        for (u32 texture_index = 0; texture_index < s_cast<u32>(asset->textures.size()); texture_index++) {
            auto const texture_manifest_index = texture_index + asset_manifest.texture_manifest_offset;
            auto const & texture_manifest_entry = texture_manifest_entries.at(texture_manifest_index);

            if (!texture_manifest_entry.material_manifest_indices.empty()) {
                thread_pool->async_dispatch(
                std::make_shared<LoadTextureTask>(LoadTextureTask::TaskInfo{
                    .load_info = {
                        .asset_path = asset_manifest.path,
                        .asset = asset.get(),
                        .texture_index = texture_index,
                        .texture_manifest_index = texture_manifest_index,
                        .old_image = {},
                        .image_path = asset_manifest.path.parent_path() / asset->textures[texture_index].file_path,
                    },
                    .asset_processor = asset_processor,
                }), TaskPriority::LOW);
            }
        }
    }

    void AssetManager::update_textures() {
        if(texture_sizes.empty()) { return; }
        std::fill(texture_sizes.begin(), texture_sizes.end(), 16);
        for(u32 texture_index = 0; texture_index < texture_manifest_entries.size(); texture_index++) {
            TextureManifestEntry& texture_manifest = texture_manifest_entries[texture_index];
            for(const auto& material_index : texture_manifest.material_manifest_indices) {
                texture_sizes[texture_index] = std::max(texture_sizes[texture_index], readback_material[material_index.material_manifest_index]);
            }
            texture_sizes[texture_index] = std::min(texture_manifest.max_resolution, texture_sizes[texture_index]);

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


            texture_manifest.unload_delay++;
            const u32 requested_size = texture_sizes[texture_index];
            if(texture_manifest.image_id.is_empty()) { continue; }
            if(requested_size == texture_manifest.current_resolution) { continue; }
            if(texture_manifest.loading) { continue; }
            if(requested_size < texture_manifest.current_resolution && texture_manifest.unload_delay < 254) { continue; }
            
            texture_manifest.loading = true;
            texture_manifest.unload_delay = 0;
            texture_manifest.current_resolution = requested_size;

            const AssetManifestEntry& asset_entry = asset_manifest_entries[texture_manifest.asset_manifest_index];
            thread_pool->async_dispatch(
                std::make_shared<LoadTextureTask>(LoadTextureTask::TaskInfo{
                    .load_info = {
                        .asset_path = asset_entry.path,
                        .asset = asset_entry.asset.get(),
                        .texture_index = texture_manifest.asset_local_index,
                        .texture_manifest_index = texture_index,
                        .requested_resolution = requested_size,
                        .old_image = texture_manifest.image_id,
                        .image_path = asset_entry.path.parent_path() / asset_entry.asset->textures[texture_manifest.asset_local_index].file_path,
                    },
                    .asset_processor = asset_processor,
            }), TaskPriority::LOW);
        }
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
        realloc_special(gpu_culled_meshlet_indices, total_meshlet_count * sizeof(u32) + sizeof(MeshletIndices), [&](const daxa::BufferId& buffer) -> MeshletIndices {
            return MeshletIndices {
                .count = 0,
                .indices = context->device.get_device_address(buffer).value() + sizeof(MeshletIndices)
            };
        });
        realloc_special(gpu_meshlet_index_buffer, total_triangle_count * sizeof(u32) + sizeof(MeshletIndexBuffer), [&](const daxa::BufferId& buffer) -> MeshletIndexBuffer {
            return MeshletIndexBuffer {
                .count = 0,
                .indices = context->device.get_device_address(buffer).value() + sizeof(MeshletIndexBuffer)
            };
        });
        readback_material.resize(material_manifest_entries.size());
        texture_sizes.resize(texture_manifest_entries.size());
        realloc(gpu_readback_material, s_cast<u32>(material_manifest_entries.size() * sizeof(u32)));

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

        if(!info.uploaded_meshes.empty()) {
            daxa::BufferId material_null_buffer = context->create_buffer({
                .size = s_cast<u32>(sizeof(Material)),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "material null buffer",
            });
            context->destroy_buffer_deferred(cmd_recorder, material_null_buffer);
            {
                Material material = {};
                material.metallic_factor = 1.0f;
                material.roughness_factor = 1.0f;
                material.emissive_factor = { 0.0f, 0.0f, 0.0f };
                material.alpha_mode = 0;
                material.alpha_cutoff = 0.5f;
                material.double_sided = 0u;

                std::memcpy(context->device.get_host_address_as<Material>(material_null_buffer).value(), &material, sizeof(Material));
            }

            for(const auto& mesh_upload_info : info.uploaded_meshes) {
                auto& mesh_manifest = mesh_manifest_entries[mesh_upload_info.manifest_index];
                auto& asset_manifest = asset_manifest_entries[mesh_manifest.asset_manifest_index];
                u32 material_index = mesh_upload_info.material_manifest_offset + asset_manifest.asset->meshes[asset_manifest.asset->mesh_groups[mesh_manifest.asset_local_mesh_index].mesh_offset + mesh_manifest.asset_local_primitive_index].material_index.value();
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
        for(const auto& texture_upload_info : info.uploaded_textures) {
            auto& texture_manifest = texture_manifest_entries.at(texture_upload_info.manifest_index);
            texture_manifest.current_resolution = context->device.info_image(texture_upload_info.dst_image).value().size.x;
            texture_manifest.image_id = texture_upload_info.dst_image;
            texture_manifest.sampler_id = texture_upload_info.sampler;
            texture_manifest.loading = false;

            for(auto& material_using_texture_info : texture_manifest.material_manifest_indices) {
                MaterialManifestEntry & material_entry = material_manifest_entries.at(material_using_texture_info.material_manifest_index);
                if (material_using_texture_info.material_type == MaterialType::GltfAlbedo) {
                    material_entry.albedo_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.material_type == MaterialType::CompressedAlphaMask) {
                    material_entry.alpha_mask_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.material_type == MaterialType::GltfNormal) {
                    material_entry.normal_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.material_type == MaterialType::CompressedRoughness) {
                    material_entry.roughness_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.material_type == MaterialType::CompressedMetalness) {
                    material_entry.metalness_info->texture_manifest_index = texture_upload_info.manifest_index;
                }
                if (material_using_texture_info.material_type == MaterialType::GltfEmissive) {
                    material_entry.emissive_info->texture_manifest_index = texture_upload_info.manifest_index;
                }

                if (std::find(dirty_material_manifest_indices.begin(), dirty_material_manifest_indices.end(), material_using_texture_info.material_manifest_index) == dirty_material_manifest_indices.end()) {
                    dirty_material_manifest_indices.push_back(material_using_texture_info.material_manifest_index);
                }
            }
        }

        if(!dirty_material_manifest_indices.empty()) {
            daxa::BufferId staging_buffer = context->create_buffer({
                .size = s_cast<u32>(dirty_material_manifest_indices.size() * sizeof(Material)),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer material manifest",
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            Material* ptr = context->device.get_host_address_as<Material>(staging_buffer).value();
            for (u32 dirty_materials_index = 0; dirty_materials_index < dirty_material_manifest_indices.size(); dirty_materials_index++) {
                MaterialManifestEntry& material = material_manifest_entries.at(dirty_material_manifest_indices.at(dirty_materials_index));
                Material gpu_material = {};
                gpu_material.metallic_factor = material.metallic_factor;
                gpu_material.roughness_factor = material.roughness_factor;
                gpu_material.emissive_factor = material.emissive_factor;
                gpu_material.alpha_mode = material.alpha_mode;
                gpu_material.alpha_cutoff = material.alpha_cutoff;
                gpu_material.double_sided = s_cast<b32>(material.double_sided);

                if (material.albedo_info.has_value()) {
                    auto const & texture_manifest = texture_manifest_entries.at(material.albedo_info.value().texture_manifest_index);
                    gpu_material.albedo_image_id = texture_manifest.image_id.default_view();
                    gpu_material.albedo_sampler_id = texture_manifest.sampler_id;
                }
                if (material.alpha_mask_info.has_value()) {
                    auto const & texture_manifest = texture_manifest_entries.at(material.alpha_mask_info.value().texture_manifest_index);
                    gpu_material.alpha_mask_image_id = texture_manifest.image_id.default_view();
                    gpu_material.alpha_mask_sampler_id = texture_manifest.sampler_id;
                }
                if (material.normal_info.has_value()) {
                    auto const & texture_manifest = texture_manifest_entries.at(material.normal_info.value().texture_manifest_index);
                    gpu_material.normal_image_id = texture_manifest.image_id.default_view();
                    gpu_material.normal_sampler_id = texture_manifest.sampler_id;
                }
                if (material.roughness_info.has_value()) {
                    auto const & texture_manifest = texture_manifest_entries.at(material.roughness_info.value().texture_manifest_index);
                    gpu_material.roughness_image_id = texture_manifest.image_id.default_view();
                    gpu_material.roughness_sampler_id = texture_manifest.sampler_id;
                }
                if (material.metalness_info.has_value()) {
                    auto const & texture_manifest = texture_manifest_entries.at(material.metalness_info.value().texture_manifest_index);
                    gpu_material.metalness_image_id = texture_manifest.image_id.default_view();
                    gpu_material.metalness_sampler_id = texture_manifest.sampler_id;
                }
                if (material.emissive_info.has_value()) {
                    auto const & texture_manifest = texture_manifest_entries.at(material.emissive_info.value().texture_manifest_index);
                    gpu_material.emissive_image_id = texture_manifest.image_id.default_view();
                    gpu_material.emissive_sampler_id = texture_manifest.sampler_id;
                }

                ptr[dirty_materials_index] = gpu_material;

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