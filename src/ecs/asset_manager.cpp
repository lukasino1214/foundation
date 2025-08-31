#include "asset_manager.hpp"
#include <ecs/components.hpp>
#include <graphics/helper.hpp>

#include <utility>
#include <utils/byte_utils.hpp>
#include <utils/zstd.hpp>
#include <math/decompose.hpp>
#include <utils/file_io.hpp>

namespace foundation {
    static constexpr usize MAXIMUM_MESHLET_COUNT = ~u32(0u) >> (find_msb(MAX_TRIANGLES_PER_MESHLET));

    struct LoadTextureTask : Task {
        struct TaskInfo {
            LoadTextureInfo load_info = {};
            AssetProcessor * asset_processor = {};
            u32 manifest_index = {};
        };

        TaskInfo info = {};
        explicit LoadTextureTask(TaskInfo info) : info{std::move(info)} { chunk_count = 1; }

        virtual void callback(u32 /*chunk_index*/, u32 /*thread_index*/) override {
            info.asset_processor->load_texture(info.load_info);
        };
    };

    struct LoadMeshTask : Task {
        struct TaskInfo {
            LoadMeshInfo load_info;
            AssetProcessor * asset_processor = {};
            u32 manifest_index = {};
        };

        TaskInfo info;
        LoadMeshTask(TaskInfo info) : info{std::move(info)} { chunk_count = 1; }

        virtual void callback(u32 /*chunk_index*/, u32 /*thread_index*/) override {
            info.asset_processor->load_gltf_mesh(info.load_info);
        };
    };

    AssetManager::AssetManager(Context* _context, Scene* _scene, ThreadPool* _thread_pool, AssetProcessor* _asset_processor) : context{_context}, scene{_scene}, thread_pool{_thread_pool}, asset_processor{_asset_processor} {
        PROFILE_SCOPE;
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

        gpu_culled_meshes_data = make_task_buffer(context, {
            sizeof(MeshesData),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "culled meshes data"
        });

        gpu_readback_material_gpu = make_task_buffer(context, {
            sizeof(u32),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "readback material gpu"
        });

        gpu_readback_material_cpu = make_task_buffer(context, {
            sizeof(u32),
            daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            "readback material cpu"
        });

        gpu_readback_mesh_gpu = make_task_buffer(context, {
            sizeof(u32),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "readback mesh gpu"
        });

        gpu_readback_mesh_cpu = make_task_buffer(context, {
            sizeof(u32),
            daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
            "readback mesh cpu"
        });

        gpu_meshlets_instance_data = make_task_buffer(context, {
            sizeof(MeshletInstanceData),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "meshlets instance data"
        });

        gpu_meshlets_data_merged_opaque = make_task_buffer(context, {
            sizeof(MeshletsDataMerged),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "meshlets data merged opaque"
        });

        gpu_meshlets_data_merged_masked = make_task_buffer(context, {
            sizeof(MeshletsDataMerged),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "meshlets data merged masked"
        });

        gpu_meshlets_data_merged_transparent = make_task_buffer(context, {
            sizeof(MeshletsDataMerged),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "meshlets data merged transparent"
        });

        gpu_opaque_prefix_sum_work_expansion_mesh = make_task_buffer(context, {
            sizeof(PrefixSumWorkExpansion),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "opaque prefix sum work expansion mesh"
        });

        gpu_masked_prefix_sum_work_expansion_mesh = make_task_buffer(context, {
            sizeof(PrefixSumWorkExpansion),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "masked prefix sum work expansion mesh"
        });

        gpu_transparent_prefix_sum_work_expansion_mesh = make_task_buffer(context, {
            sizeof(PrefixSumWorkExpansion),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "transparent prefix sum work expansion mesh"
        });
    }
    AssetManager::~AssetManager() {
        context->destroy_buffer(gpu_meshes.get_state().buffers[0]);
        context->destroy_buffer(gpu_materials.get_state().buffers[0]);
        context->destroy_buffer(gpu_mesh_groups.get_state().buffers[0]);
        context->destroy_buffer(gpu_mesh_indices.get_state().buffers[0]);
        context->destroy_buffer(gpu_meshlets_instance_data.get_state().buffers[0]);
        context->destroy_buffer(gpu_meshlets_data_merged_opaque.get_state().buffers[0]);
        context->destroy_buffer(gpu_meshlets_data_merged_masked.get_state().buffers[0]);
        context->destroy_buffer(gpu_meshlets_data_merged_transparent.get_state().buffers[0]);
        context->destroy_buffer(gpu_culled_meshes_data.get_state().buffers[0]);
        context->destroy_buffer(gpu_readback_material_gpu.get_state().buffers[0]);
        context->destroy_buffer(gpu_readback_material_cpu.get_state().buffers[0]);
        context->destroy_buffer(gpu_readback_mesh_gpu.get_state().buffers[0]);
        context->destroy_buffer(gpu_readback_mesh_cpu.get_state().buffers[0]);
        context->destroy_buffer(gpu_opaque_prefix_sum_work_expansion_mesh.get_state().buffers[0]);
        context->destroy_buffer(gpu_masked_prefix_sum_work_expansion_mesh.get_state().buffers[0]);
        context->destroy_buffer(gpu_transparent_prefix_sum_work_expansion_mesh.get_state().buffers[0]);

        for(auto& mesh_manifest : mesh_manifest_entries) {
            if(context->device.is_buffer_id_valid(mesh_manifest.virtual_geometry_render_info->mesh.mesh_buffer)) {
                context->destroy_buffer(mesh_manifest.virtual_geometry_render_info->mesh.mesh_buffer);
            }
        }

        for(auto& texture_manifest : texture_manifest_entries) {
            if(context->device.is_image_id_valid(texture_manifest.image_id)) {
                context->destroy_image(texture_manifest.image_id);
            }
        }
    }

    void AssetManager::load_model(LoadManifestInfo& info) {
        PROFILE_SCOPE_NAMED(load_model);
        if(!std::filesystem::exists(info.path)) {
            throw std::runtime_error("couldnt not find model: " + info.path.string());
        }

        auto* mc = info.parent.add_component<ModelComponent>();
        mc->path = info.path;

        bool asset_isnt_in_memory = true;
        const AssetManifestEntry* asset_manifest = {};
        const BinaryAssetInfo* asset = {};

        for(const auto& asset_manifest_ : asset_manifest_entries) {
            if(info.path == asset_manifest_.path) {
                asset_isnt_in_memory = false;
                asset_manifest = std::addressof(asset_manifest_);
                asset = asset_manifest->asset.get();
            }
        }

        if(asset_isnt_in_memory) {
            u32 const asset_manifest_index = s_cast<u32>(asset_manifest_entries.size());
            u32 const texture_manifest_offset = s_cast<u32>(texture_manifest_entries.size());
            u32 const material_manifest_offset = s_cast<u32>(material_manifest_entries.size());
            u32 const mesh_manifest_offset = s_cast<u32>(mesh_manifest_entries.size());
            u32 const mesh_group_manifest_offset = s_cast<u32>(mesh_group_manifest_entries.size());

            std::filesystem::path binary_path = info.path;

            {
                std::vector<byte> data = read_file_to_bytes(binary_path);
                std::vector<byte> uncompressed_data = zstd_decompress(data);

                ByteReader byte_reader{ uncompressed_data.data(), uncompressed_data.size() };

                BinaryModelHeader header = {};
                byte_reader.read(header);

                total_meshlet_count += header.meshlet_count;
                total_triangle_count += header.triangle_count;
                total_vertex_count += header.vertex_count;

                BinaryAssetInfo binary_asset = {};
                byte_reader.read(binary_asset);
                
                total_unique_mesh_count += s_cast<u32>(binary_asset.meshes.size());

                asset_manifest_entries.push_back(AssetManifestEntry {
                    .path = info.path,
                    .texture_manifest_offset = texture_manifest_offset,
                    .material_manifest_offset = material_manifest_offset,
                    .mesh_manifest_offset = mesh_manifest_offset,
                    .mesh_group_manifest_offset = mesh_group_manifest_offset,
                    .parent = info.parent,
                    .asset = std::make_unique<BinaryAssetInfo>(std::move(binary_asset)),
                    .header = header
                });
            }

            asset_manifest = &asset_manifest_entries.back();
            asset = asset_manifest->asset.get();

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
                    .albedo_factor = material.albedo_factor,
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
        }

        total_meshlet_count += asset_manifest->header.meshlet_count;
        total_triangle_count += asset_manifest->header.triangle_count;
        total_vertex_count += asset_manifest->header.vertex_count;
        total_mesh_count += s_cast<u32>(asset->meshes.size());

        for(u32 mesh_group_index = 0; mesh_group_index < asset->mesh_groups.size(); mesh_group_index++) {
            const auto& mesh_group = asset->mesh_groups.at(mesh_group_index);

            for(u32 mesh_index = 0; mesh_index < mesh_group.mesh_count; mesh_index++) {
                const BinaryMesh& binary_mesh = asset->meshes[mesh_group.mesh_offset + mesh_index];
        
                if(binary_mesh.material_index.has_value()) {
                    const BinaryMaterial& binary_material = asset->materials[binary_mesh.material_index.value()];
        
                    switch (binary_material.alpha_mode) {
                        case BinaryAlphaMode::Opaque: { 
                            total_opaque_mesh_count++; 
                            total_opaque_meshlet_count += binary_mesh.meshlet_count;
                            break; 
                        }
                        case BinaryAlphaMode::Masked: { 
                            total_masked_mesh_count++; 
                            total_masked_meshlet_count += binary_mesh.meshlet_count;
                            break; }
                        case BinaryAlphaMode::Transparent: { 
                            total_transparent_mesh_count++; 
                            total_transparent_meshlet_count += binary_mesh.meshlet_count;
                            break; 
                        }
                        default: { 
                            std::println("something went wrong. so boom!"); 
                            std::exit(-1); 
                            break; 
                        }
                    }
                } else {
                    total_opaque_meshlet_count += binary_mesh.meshlet_count;
                    total_opaque_mesh_count++;
                }
            }
        }

        std::vector<Entity> node_index_to_entity = {};
        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            node_index_to_entity.push_back(scene->create_entity(std::format("asset {} {} {}", asset_manifest_entries.size(), asset->nodes[node_index].name, info.parent.get_name().data())));
        }

        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            const auto& node = asset->nodes[node_index];
            Entity& parent_entity = node_index_to_entity[node_index];
            if(node.mesh_index.has_value()) {
                auto* mesh_component = parent_entity.add_component<MeshComponent>();
                mesh_component->mesh_group_index = asset_manifest->mesh_group_manifest_offset + node.mesh_index.value();
                parent_entity.add_component<RenderInfo>();
            }

            glm::vec3 position = {};
            glm::vec3 rotation = {};
            glm::vec3 scale = {};
            math::decompose_transform(node.transform, position, rotation, scale);

            parent_entity.add_component<TransformComponent>();
            parent_entity.set_local_position(position);
            parent_entity.set_local_rotation(rotation);
            parent_entity.set_local_scale(scale);

            for(u32 children_index = 0; children_index < node.children.size(); children_index++) {
                Entity& child_entity = node_index_to_entity[children_index];
                parent_entity.set_child(child_entity);
            }
        }

        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            Entity& entity = node_index_to_entity[node_index];

            if(!entity.has_parent()) {
                info.parent.set_child(entity);
            }
        }

        if(asset_isnt_in_memory) {
            for(u32 mesh_group_index = 0; mesh_group_index < asset->mesh_groups.size(); mesh_group_index++) {
                const auto& mesh_group = asset->mesh_groups.at(mesh_group_index);
                const auto& mesh_group_manifest = mesh_group_manifest_entries[asset_manifest->mesh_group_manifest_offset + mesh_group_index];
                for(u32 mesh_index = 0; mesh_index < mesh_group.mesh_count; mesh_index++) {
                    thread_pool->async_dispatch(std::make_shared<LoadMeshTask>(LoadMeshTask::TaskInfo{
                        .load_info = {
                            .asset_path = info.path,
                            .asset = asset,
                            .mesh_group_index = mesh_group_index,
                            .mesh_index = mesh_index,
                            .material_manifest_offset = asset_manifest->material_manifest_offset,
                            .manifest_index = mesh_group_manifest.mesh_manifest_indices_offset + mesh_index,
                            .old_mesh = {},
                            .file_path = asset_manifest->path.parent_path() / asset->meshes[mesh_group.mesh_offset + mesh_index].file_path
                        },
                        .asset_processor = asset_processor,
                    }), TaskPriority::LOW);
                }
            }

            for (u32 texture_index = 0; texture_index < s_cast<u32>(asset->textures.size()); texture_index++) {
                auto const texture_manifest_index = texture_index + asset_manifest->texture_manifest_offset;
                auto const & texture_manifest_entry = texture_manifest_entries.at(texture_manifest_index);

                if (!texture_manifest_entry.material_manifest_indices.empty()) {
                    thread_pool->async_dispatch(
                    std::make_shared<LoadTextureTask>(LoadTextureTask::TaskInfo{
                        .load_info = {
                            .asset_path = asset_manifest->path,
                            .asset = asset,
                            .texture_index = texture_index,
                            .texture_manifest_index = texture_manifest_index,
                            .old_image = {},
                            .image_path = asset_manifest->path.parent_path() / asset->textures[texture_index].file_path,
                        },
                        .asset_processor = asset_processor,
                    }), TaskPriority::LOW);
                }
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

    void AssetManager::update_meshes() {
        if(readback_mesh.empty()) { return; }

        for(u32 global_mesh_index = 0; global_mesh_index < mesh_manifest_entries.size(); global_mesh_index++) {
            MeshManifestEntry& mesh_manifest_entry = mesh_manifest_entries[global_mesh_index];
            const bool keep_mesh_in_memory = (readback_mesh[global_mesh_index / 32u] & (1u << (global_mesh_index % 32u))) != 0;

            const AssetManifestEntry& asset_manifest = asset_manifest_entries[mesh_manifest_entry.asset_manifest_index];
            const u32 mesh_group_index = asset_manifest.mesh_group_manifest_offset + mesh_manifest_entry.asset_local_mesh_index;
            const MeshGroupManifestEntry mesh_group_manifest_entry = mesh_group_manifest_entries[mesh_group_index];
            const auto& mesh_group = asset_manifest.asset->mesh_groups.at(mesh_manifest_entry.asset_local_mesh_index);

            if(!mesh_manifest_entry.virtual_geometry_render_info.has_value()) { continue; }
            const daxa::BufferId mesh_buffer = mesh_manifest_entry.virtual_geometry_render_info->mesh.mesh_buffer;
            bool is_in_memory = !mesh_buffer.is_empty() && context->device.is_buffer_id_valid(mesh_buffer);
            
            mesh_manifest_entry.unload_delay++;
            
            if(mesh_manifest_entry.unload_delay < 254) { continue; }
            if(mesh_manifest_entry.loading) { continue; }
            if(keep_mesh_in_memory == is_in_memory) { continue; }

            mesh_manifest_entry.loading = true;
            mesh_manifest_entry.unload_delay = 0;

            thread_pool->async_dispatch(std::make_shared<LoadMeshTask>(LoadMeshTask::TaskInfo{
                .load_info = {
                    .asset_path = asset_manifest.path,
                    .asset = asset_manifest.asset.get(),
                    .mesh_group_index = mesh_manifest_entry.asset_local_mesh_index,
                    .mesh_index = mesh_manifest_entry.asset_local_primitive_index,
                    .material_manifest_offset = asset_manifest.material_manifest_offset,
                    .manifest_index = mesh_group_manifest_entry.mesh_manifest_indices_offset + mesh_manifest_entry.asset_local_primitive_index,
                    .old_mesh = mesh_manifest_entry.virtual_geometry_render_info->mesh,
                    .file_path = asset_manifest.path.parent_path() / asset_manifest.asset->meshes[mesh_group.mesh_offset + mesh_manifest_entry.asset_local_primitive_index].file_path
                },
                .asset_processor = asset_processor,
            }), TaskPriority::LOW);
        }
    }

    auto AssetManager::record_manifest_update(const RecordManifestUpdateInfo& info) -> daxa::ExecutableCommandList {
        PROFILE_SCOPE;
        auto cmd_recorder = context->device.create_command_recorder({ .name = "asset manager update" });

        reallocate_buffer(context, cmd_recorder, gpu_meshes, s_cast<u32>(mesh_manifest_entries.size() * sizeof(Mesh)));
        reallocate_buffer(context, cmd_recorder, gpu_materials, s_cast<u32>(material_manifest_entries.size() * sizeof(Material)));
        reallocate_buffer(context, cmd_recorder, gpu_mesh_groups, s_cast<u32>(mesh_group_manifest_entries.size() * sizeof(MeshGroup)));
        reallocate_buffer(context, cmd_recorder, gpu_mesh_indices, s_cast<u32>(mesh_manifest_entries.size() * sizeof(u32)));
        reallocate_buffer<MeshesData>(context, cmd_recorder, gpu_culled_meshes_data, s_cast<u32>(total_mesh_count * sizeof(MeshData) + sizeof(MeshesData)), [&](const daxa::BufferId& buffer) -> MeshesData {
            return MeshesData {
                .count = 0,
                .meshes = context->device.buffer_device_address(buffer).value() + sizeof(MeshesData)
            };
        });

        if(reallocate_buffer(context, cmd_recorder, gpu_meshlets_instance_data, 2 * total_meshlet_count * sizeof(MeshletInstanceData))) {
            daxa::DeviceAddress buffer_address = context->device.buffer_device_address(gpu_meshlets_instance_data.get_state().buffers[0]).value();
            
            u32 hw_offset = 0;
            u32 sw_offset = s_cast<u32>(total_opaque_meshlet_count);
            {
                daxa::BufferId stagging_buffer = context->create_buffer(daxa::BufferInfo {
                    .size = sizeof(MeshletsDataMerged),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "opaque stagging buffer"
                });
                context->destroy_buffer_deferred(cmd_recorder, stagging_buffer);

                MeshletsDataMerged data {
                    .hw_count = 0,
                    .sw_count = 0,
                    .hw_offset = hw_offset,
                    .sw_offset = sw_offset,
                    .hw_meshlet_data = buffer_address + hw_offset * sizeof(MeshletInstanceData),
                    .sw_meshlet_data = buffer_address + sw_offset * sizeof(MeshletInstanceData),
                };

                std::memcpy(context->device.buffer_host_address(stagging_buffer).value(), &data, sizeof(MeshletsDataMerged));
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = stagging_buffer,
                    .dst_buffer = gpu_meshlets_data_merged_opaque.get_state().buffers[0],
                    .size = sizeof(MeshletsDataMerged),
                });
            }
            hw_offset = 2 * s_cast<u32>(total_opaque_meshlet_count);
            sw_offset = 2 * s_cast<u32>(total_opaque_meshlet_count) + s_cast<u32>(total_masked_meshlet_count);
            {
                daxa::BufferId stagging_buffer = context->create_buffer(daxa::BufferInfo {
                    .size = sizeof(MeshletsDataMerged),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "masked stagging buffer"
                });
                context->destroy_buffer_deferred(cmd_recorder, stagging_buffer);

                MeshletsDataMerged data {
                    .hw_count = 0,
                    .sw_count = 0,
                    .hw_offset = hw_offset,
                    .sw_offset = sw_offset,
                    .hw_meshlet_data = buffer_address + hw_offset * sizeof(MeshletInstanceData),
                    .sw_meshlet_data = buffer_address + sw_offset * sizeof(MeshletInstanceData),
                };

                std::memcpy(context->device.buffer_host_address(stagging_buffer).value(), &data, sizeof(MeshletsDataMerged));
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = stagging_buffer,
                    .dst_buffer = gpu_meshlets_data_merged_masked.get_state().buffers[0],
                    .size = sizeof(MeshletsDataMerged),
                });
            }
            hw_offset = 2 * s_cast<u32>(total_opaque_meshlet_count) + 2 * s_cast<u32>(total_masked_meshlet_count);
            sw_offset = 2 * s_cast<u32>(total_opaque_meshlet_count) + 2 * s_cast<u32>(total_masked_meshlet_count) + s_cast<u32>(total_transparent_meshlet_count);
            {
                daxa::BufferId stagging_buffer = context->create_buffer(daxa::BufferInfo {
                    .size = sizeof(MeshletsDataMerged),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "transparent stagging buffer"
                });
                context->destroy_buffer_deferred(cmd_recorder, stagging_buffer);

                MeshletsDataMerged data {
                    .hw_count = 0,
                    .sw_count = 0,
                    .hw_offset = hw_offset,
                    .sw_offset = sw_offset,
                    .hw_meshlet_data = buffer_address + hw_offset * sizeof(MeshletInstanceData),
                    .sw_meshlet_data = buffer_address + sw_offset * sizeof(MeshletInstanceData),
                };

                std::memcpy(context->device.buffer_host_address(stagging_buffer).value(), &data, sizeof(MeshletsDataMerged));
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = stagging_buffer,
                    .dst_buffer = gpu_meshlets_data_merged_transparent.get_state().buffers[0],
                    .size = sizeof(MeshletsDataMerged),
                });
            }
        }

        reallocate_buffer<PrefixSumWorkExpansion>(context, cmd_recorder, gpu_opaque_prefix_sum_work_expansion_mesh, total_opaque_mesh_count * sizeof(u32) * 3 + sizeof(PrefixSumWorkExpansion), [&](const daxa::BufferId& buffer) -> PrefixSumWorkExpansion {
            return PrefixSumWorkExpansion {
                .merged_expansion_count_thread_count = 0,
                .expansions_max = s_cast<u32>(total_mesh_count),
                .workgroup_count = 0,
                .expansions_inclusive_prefix_sum = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_mesh_count * sizeof(u32) * 0,
                .expansions_src_work_item = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_mesh_count * sizeof(u32) * 1,
                .expansions_expansion_factor = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_mesh_count * sizeof(u32) * 2
            };
        });
        reallocate_buffer<PrefixSumWorkExpansion>(context, cmd_recorder, gpu_masked_prefix_sum_work_expansion_mesh, total_masked_mesh_count * sizeof(u32) * 3 + sizeof(PrefixSumWorkExpansion), [&](const daxa::BufferId& buffer) -> PrefixSumWorkExpansion {
            return PrefixSumWorkExpansion {
                .merged_expansion_count_thread_count = 0,
                .expansions_max = s_cast<u32>(total_masked_mesh_count),
                .workgroup_count = 0,
                .expansions_inclusive_prefix_sum = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_masked_mesh_count * sizeof(u32) * 0,
                .expansions_src_work_item = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_masked_mesh_count * sizeof(u32) * 1,
                .expansions_expansion_factor = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_masked_mesh_count * sizeof(u32) * 2
            };
        });
        reallocate_buffer<PrefixSumWorkExpansion>(context, cmd_recorder, gpu_transparent_prefix_sum_work_expansion_mesh, total_transparent_mesh_count * sizeof(u32) * 3 + sizeof(PrefixSumWorkExpansion), [&](const daxa::BufferId& buffer) -> PrefixSumWorkExpansion {
            return PrefixSumWorkExpansion {
                .merged_expansion_count_thread_count = 0,
                .expansions_max = s_cast<u32>(total_transparent_mesh_count),
                .workgroup_count = 0,
                .expansions_inclusive_prefix_sum = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_transparent_mesh_count * sizeof(u32) * 0,
                .expansions_src_work_item = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_transparent_mesh_count * sizeof(u32) * 1,
                .expansions_expansion_factor = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_transparent_mesh_count * sizeof(u32) * 2
            };
        });

        const u32 readback_mesh_size = round_up_div(s_cast<u32>(mesh_manifest_entries.size()), 32u);

        readback_material.resize(material_manifest_entries.size());
        texture_sizes.resize(texture_manifest_entries.size());
        readback_mesh.resize(readback_mesh_size);
        reallocate_buffer(context, cmd_recorder, gpu_readback_material_gpu, s_cast<u32>(material_manifest_entries.size() * sizeof(u32)));
        reallocate_buffer(context, cmd_recorder, gpu_readback_material_cpu, s_cast<u32>(material_manifest_entries.size() * sizeof(u32)));
        reallocate_buffer(context, cmd_recorder, gpu_readback_mesh_gpu, readback_mesh_size * sizeof(u32));
        reallocate_buffer(context, cmd_recorder, gpu_readback_mesh_cpu, readback_mesh_size * sizeof(u32));

        if(!dirty_meshes.empty()) {
            Mesh mesh = {};

            daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer meshes"
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            std::memcpy(context->device.buffer_host_address(staging_buffer).value(), &mesh, sizeof(Mesh));

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
            u64 buffer_ptr = context->device.buffer_device_address(gpu_mesh_indices.get_state().buffers[0]).value();
            mesh_groups.reserve(dirty_mesh_groups.size());
            for(u32 mesh_group_index : dirty_mesh_groups) {
                const auto& mesh_group = mesh_group_manifest_entries[mesh_group_index];
                
                mesh_groups.push_back(MeshGroup {
                    .mesh_indices = buffer_ptr + mesh_group.mesh_manifest_indices_offset * sizeof(u32),
                    .count = mesh_group.mesh_count,
                    .padding = {},
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
            std::byte* ptr = context->device.buffer_host_address(staging_buffer).value();

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
                material.albedo_factor = f32vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.metallic_factor = 1.0f;
                material.roughness_factor = 1.0f;
                material.emissive_factor = { 0.0f, 0.0f, 0.0f };
                material.alpha_mode = 0;
                material.alpha_cutoff = 0.5f;
                material.double_sided = 0u;

                std::memcpy(context->device.buffer_host_address_as<Material>(material_null_buffer).value(), &material, sizeof(Material));
            }

            for(const auto& mesh_upload_info : info.uploaded_meshes) {
                auto& mesh_manifest = mesh_manifest_entries[mesh_upload_info.manifest_index];
                mesh_manifest.loading = false;
                auto& asset_manifest = asset_manifest_entries[mesh_manifest.asset_manifest_index];
                const BinaryMesh& binary_mesh = asset_manifest.asset->meshes[asset_manifest.asset->mesh_groups[mesh_manifest.asset_local_mesh_index].mesh_offset + mesh_manifest.asset_local_primitive_index];

                mesh_manifest.virtual_geometry_render_info = MeshManifestEntry::VirtualGeometryRenderInfo { .mesh = mesh_upload_info.mesh, };
                if(!mesh_manifest.virtual_geometry_render_info.has_value() && binary_mesh.material_index.has_value()) {
                    u32 material_index = mesh_upload_info.material_manifest_offset + binary_mesh.material_index.value();
                    auto& material_manifest = material_manifest_entries.at(material_index);

                    cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                        .src_buffer = material_null_buffer,
                        .dst_buffer = gpu_materials.get_state().buffers[0],
                        .src_offset = 0,
                        .dst_offset = material_manifest.material_manifest_index * sizeof(Material),
                        .size = sizeof(Material),
                    });
                    mesh_manifest.virtual_geometry_render_info->material_manifest_index = material_manifest.material_manifest_index;
                }
            }

            daxa::BufferId staging_buffer = context->create_buffer({
                .size = info.uploaded_meshes.size() * sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "mesh manifest upload staging buffer",
            });

            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            Mesh * staging_ptr = context->device.buffer_host_address_as<Mesh>(staging_buffer).value();

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
            texture_manifest.current_resolution = context->device.image_info(texture_upload_info.dst_image).value().size.x;
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

        for(const auto& mesh_upload_info : info.uploaded_meshes) {
            dirty_material_manifest_indices.push_back(mesh_upload_info.mesh.material_index);
        }

        if(!dirty_material_manifest_indices.empty()) {
            daxa::BufferId staging_buffer = context->create_buffer({
                .size = s_cast<u32>(dirty_material_manifest_indices.size() * sizeof(Material)),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer material manifest",
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            Material* ptr = context->device.buffer_host_address_as<Material>(staging_buffer).value();
            for (u32 dirty_materials_index = 0; dirty_materials_index < dirty_material_manifest_indices.size(); dirty_materials_index++) {
                MaterialManifestEntry& material = material_manifest_entries.at(dirty_material_manifest_indices.at(dirty_materials_index));

                Material gpu_material = {};
                gpu_material.albedo_factor = material.albedo_factor,
                gpu_material.metallic_factor = material.metallic_factor;
                gpu_material.roughness_factor = material.roughness_factor;
                gpu_material.emissive_factor = material.emissive_factor;
                gpu_material.alpha_mode = s_cast<u32>(material.alpha_mode);
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