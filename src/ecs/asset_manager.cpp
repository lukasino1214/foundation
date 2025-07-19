#include "asset_manager.hpp"
#include <ecs/components.hpp>
#include <graphics/helper.hpp>

#include <utility>
#include <utility>
#include <utils/byte_utils.hpp>
#include <utils/zstd.hpp>
#include <math/decompose.hpp>
#include <utils/file_io.hpp>
#include <glm/gtc/quaternion.hpp>

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

        gpu_animated_meshes = make_task_buffer(context, {
            sizeof(AnimatedMeshData), 
            daxa::MemoryFlagBits::DEDICATED_MEMORY, 
            "animated meshes"
        });

        gpu_animated_mesh_vertices_prefix_sum_work_expansion = make_task_buffer(context, {
            sizeof(PrefixSumWorkExpansion),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "animated mesh vertices prefix sum work expansion"
        });

        gpu_animated_mesh_meshlets_prefix_sum_work_expansion = make_task_buffer(context, {
            sizeof(PrefixSumWorkExpansion),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "animated mesh meshlets prefix sum work expansion"
        });

        gpu_calculated_weights = make_task_buffer(context, {
            sizeof(f32),
            daxa::MemoryFlagBits::DEDICATED_MEMORY,
            "calculated weights"
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
        context->destroy_buffer(gpu_animated_meshes.get_state().buffers[0]);
        context->destroy_buffer(gpu_animated_mesh_vertices_prefix_sum_work_expansion.get_state().buffers[0]);
        context->destroy_buffer(gpu_animated_mesh_meshlets_prefix_sum_work_expansion.get_state().buffers[0]);
        context->destroy_buffer(gpu_calculated_weights.get_state().buffers[0]);
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
            if(context->device.is_buffer_id_valid(mesh_manifest.geometry_info->mesh_geometry_data.mesh_buffer)) {
                context->destroy_buffer(mesh_manifest.geometry_info->mesh_geometry_data.mesh_buffer);

                for(const AnimatedMeshInfo& animated_mesh_info : mesh_manifest.animated_mesh_indices) {
                    context->destroy_buffer(animated_mesh_info.animated_mesh_buffer);
                }
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
        AssetManifestEntry* asset_manifest = {};
        BinaryAssetInfo* asset = {};

        for(auto& asset_manifest_ : asset_manifest_entries) {
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
                    mesh_manifest_entries.push_back(MeshManifestEntry {
                        .asset_manifest_index = asset_manifest_index,
                        .asset_local_mesh_index = mesh_group_index,
                        .asset_local_primitive_index = mesh_index,
                        .geometry_info = std::nullopt
                    });
                }

                u32 weights_offset = s_cast<u32>(calculated_weights.size());
                
                if(mesh_group.has_morph_targets) {
                    calculated_weights.resize(calculated_weights.size() + mesh_group.weights.size());
                }

                mesh_group_manifest_entries.push_back(MeshGroupManifestEntry {
                    .mesh_manifest_indices_offset = mesh_manifest_indices_offset,
                    .mesh_count = mesh_group.mesh_count,
                    .asset_manifest_index = asset_manifest_index,
                    .asset_local_index = mesh_group_index,
                    .has_morph_targets = mesh_group.has_morph_targets,
                    .weights = mesh_group.weights,
                    .name = {},
                    .weights_offset = weights_offset,
                });
            }
        }

        total_meshlet_count += asset_manifest->header.meshlet_count;
        total_triangle_count += asset_manifest->header.triangle_count;
        total_vertex_count += asset_manifest->header.vertex_count;

        for(u32 mesh_group_index = 0; mesh_group_index < asset->mesh_groups.size(); mesh_group_index++) {
            const auto& mesh_group = asset->mesh_groups.at(mesh_group_index);

            dirty_mesh_groups.push_back(MeshGroupToMeshesMapping {
                .mesh_group_manifest_index = asset_manifest->mesh_group_manifest_offset + mesh_group_index,
                .mesh_group_index = s_cast<u32>(total_mesh_group_count++),
                .mesh_offset = s_cast<u32>(total_mesh_count),
            });

            for(u32 mesh_index = 0; mesh_index < mesh_group.mesh_count; mesh_index++) {
                const BinaryMesh& binary_mesh = asset->meshes[mesh_group.mesh_offset + mesh_index];
                
                MeshManifestEntry& mesh_manifest_entry = mesh_manifest_entries[asset_manifest->mesh_manifest_offset + mesh_group.mesh_offset + mesh_index];
                
                u32 gpu_mesh_index = s_cast<u32>(total_mesh_count++);
                dirty_meshes.push_back(s_cast<u32>(gpu_mesh_index));
                mesh_manifest_entry.mesh_indices.push_back(gpu_mesh_index);

                if(mesh_group.has_morph_targets) {
                    u32 animated_mesh_index = s_cast<u32>(total_animated_mesh_count++);
                    dirty_animated_meshes.push_back(s_cast<u32>(animated_mesh_index));
                    mesh_manifest_entry.animated_mesh_indices.push_back(AnimatedMeshInfo {
                        .animated_mesh_index = animated_mesh_index,
                        .animated_mesh_buffer = {},
                    });
                }

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
                            throw std::runtime_error("something went wrong");
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
            node_index_to_entity.push_back(scene->create_entity(std::format("asset {} {} {} {}", asset_manifest_entries.size(), asset->nodes[node_index].name, info.parent.get_name().data(), node_index)));
        }

        for(u32 node_index = 0; node_index < s_cast<u32>(asset->nodes.size()); node_index++) {
            const auto& node = asset->nodes[node_index];
            Entity& parent_entity = node_index_to_entity[node_index];
            if(node.mesh_index.has_value()) {
                auto* mesh_component = parent_entity.add_component<MeshComponent>();
                mesh_component->mesh_group_index = total_mesh_group_count - asset->mesh_groups.size() + node.mesh_index.value();
                mesh_component->mesh_group_manifest_entry_index = asset_manifest->mesh_group_manifest_offset + node.mesh_index.value();
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

            for(u32 children_index : node.children) {
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

        std::vector<AnimationState> animation_states = {};
        animation_states.reserve(asset->animations.size());

        for(const BinaryAnimation& animation : asset->animations) {
            f32 min_timestamp = std::numeric_limits<f32>::max();
            f32 max_timestamp = std::numeric_limits<f32>::lowest();

            for(const auto& sampler : animation.samplers) {
                min_timestamp = glm::min(min_timestamp, sampler.timestamps.front());
                max_timestamp = glm::max(max_timestamp, sampler.timestamps.back());
            }

            animation_states.push_back({
                .current_timestamp = 0.0f,
                .min_timestamp = min_timestamp,
                .max_timestamp = max_timestamp,
            });
        }

        asset_manifest->entity_asset_datas.push_back({
            .entity = info.parent,
            .node_index_to_entity = node_index_to_entity,
            .animation_states = animation_states,
        });

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
                            // .old_mesh = {},
                            .file_path = asset_manifest->path.parent_path() / asset->meshes[mesh_group.mesh_offset + mesh_index].file_path,
                            .is_animated = mesh_group_manifest.has_morph_targets,
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
        // if(readback_mesh.empty()) { return; }

        // for(u32 global_mesh_index = 0; global_mesh_index < mesh_manifest_entries.size(); global_mesh_index++) {
        //     MeshManifestEntry& mesh_manifest_entry = mesh_manifest_entries[global_mesh_index];
        //     const bool keep_mesh_in_memory = (readback_mesh[global_mesh_index / 32u] & (1u << (global_mesh_index % 32u))) != 0;

        //     const AssetManifestEntry& asset_manifest = asset_manifest_entries[mesh_manifest_entry.asset_manifest_index];
        //     const u32 mesh_group_index = asset_manifest.mesh_group_manifest_offset + mesh_manifest_entry.asset_local_mesh_index;
        //     const MeshGroupManifestEntry mesh_group_manifest_entry = mesh_group_manifest_entries[mesh_group_index];
        //     const auto& mesh_group = asset_manifest.asset->mesh_groups.at(mesh_manifest_entry.asset_local_mesh_index);

        //     if(!mesh_manifest_entry.virtual_geometry_render_info.has_value()) { continue; }
        //     const daxa::BufferId mesh_buffer = mesh_manifest_entry.virtual_geometry_render_info->mesh.mesh_buffer;
        //     bool is_in_memory = !mesh_buffer.is_empty() && context->device.is_buffer_id_valid(mesh_buffer);
            
        //     mesh_manifest_entry.unload_delay++;
            
        //     if(mesh_manifest_entry.unload_delay < 254) { continue; }
        //     if(mesh_manifest_entry.loading) { continue; }
        //     if(keep_mesh_in_memory == is_in_memory) { continue; }

        //     mesh_manifest_entry.loading = true;
        //     mesh_manifest_entry.unload_delay = 0;

        //     thread_pool->async_dispatch(std::make_shared<LoadMeshTask>(LoadMeshTask::TaskInfo{
        //         .load_info = {
        //             .asset_path = asset_manifest.path,
        //             .asset = asset_manifest.asset.get(),
        //             .mesh_group_index = mesh_manifest_entry.asset_local_mesh_index,
        //             .mesh_index = mesh_manifest_entry.asset_local_primitive_index,
        //             .material_manifest_offset = asset_manifest.material_manifest_offset,
        //             .manifest_index = mesh_group_manifest_entry.mesh_manifest_indices_offset + mesh_manifest_entry.asset_local_primitive_index,
        //             .old_mesh = mesh_manifest_entry.virtual_geometry_render_info->mesh,
        //             .file_path = asset_manifest.path.parent_path() / asset_manifest.asset->meshes[mesh_group.mesh_offset + mesh_manifest_entry.asset_local_primitive_index].file_path
        //         },
        //         .asset_processor = asset_processor,
        //     }), TaskPriority::LOW);
        // }
    }

    void AssetManager::update_animations(f32 delta_time) {
        for(auto& asset_manifest : asset_manifest_entries) {
            const auto& asset = asset_manifest.asset;
            if(asset->animations.empty()) { continue; }

            for(auto& entity_data : asset_manifest.entity_asset_datas) {
                auto& animation_states = entity_data.animation_states;

                for(u32 animation_index = 0; animation_index < asset->animations.size(); animation_index++) {
                    const BinaryAnimation& animation = asset->animations[animation_index];
                    AnimationState& animation_state = animation_states[animation_index];
                    
                    animation_state.current_timestamp += delta_time;
                    f32& current_timestamp = animation_state.current_timestamp;
                    
                    if(current_timestamp <= animation_state.min_timestamp) {
                        current_timestamp = animation_state.min_timestamp;
                    }
    
                    if(current_timestamp >= animation_state.max_timestamp) {
                        current_timestamp = animation_state.min_timestamp;
                    }
                    
                    for(const auto& channel : animation.channels) {
                        const auto& sampler = animation.samplers[channel.sampler];
                        
                        Entity entity = entity_data.node_index_to_entity[channel.node];
                        MeshGroupManifestEntry* mesh_group = {};
                        if(entity.has_component<MeshComponent>()) {
                            auto* c = entity.get_component<MeshComponent>();
                            if(c->mesh_group_manifest_entry_index.has_value()) {
                                mesh_group = &mesh_group_manifest_entries[c->mesh_group_manifest_entry_index.value()];
                            }
                        }
    
                        const f32* f32_values = r_cast<const f32*>(sampler.values.data());
                        const f32vec3* f32vec3_values = r_cast<const f32vec3*>(sampler.values.data());
                        const f32vec4* f32vec4_values = r_cast<const f32vec4*>(sampler.values.data());
    
                        for(u32 i = 0; i < sampler.timestamps.size() - 1; i++) {
                            if ((current_timestamp >= sampler.timestamps[i]) && (current_timestamp <= sampler.timestamps[i + 1])) {
                                switch (sampler.interpolation) {
                                    case BinaryAnimation::InterpolationType::Linear: {
                                        f32 alpha = (current_timestamp - sampler.timestamps[i]) / (sampler.timestamps[i + 1] - sampler.timestamps[i]);
                                        
                                        switch (channel.path) {
                                            case BinaryAnimation::PathType::Position: {
                                                f32vec3 position = glm::mix(f32vec3_values[i], f32vec3_values[i + 1], alpha);
                                                entity.set_local_position(position);
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Rotation: {
                                                f32vec4 value_1 = f32vec4_values[i];
                                                f32vec4 value_2 = f32vec4_values[i + 1];
    
                                                glm::quat quat = glm::normalize(glm::slerp(
                                                    glm::quat(value_1.w, value_1.x, value_1.y, value_1.z), 
                                                    glm::quat(value_2.w, value_2.x, value_2.y, value_2.z), 
                                                    alpha
                                                ));
                                                
                                                entity.set_local_rotation(glm::degrees(glm::eulerAngles(quat)));
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Scale: {
                                                f32vec3 scale = glm::mix(f32vec3_values[i], f32vec3_values[i + 1], alpha);
                                                entity.set_local_scale(scale);
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Weights: {
                                                const u32 morph_target_offset = s_cast<u32>(mesh_group->weights.size());
    
                                                for(u32 morph_target = 0; morph_target < mesh_group->weights.size(); morph_target++) {
                                                    f32 weight = glm::mix(
                                                        f32_values[morph_target_offset * i + morph_target], 
                                                        f32_values[morph_target_offset * (i + 1) + morph_target], 
                                                        alpha
                                                    );

                                                    calculated_weights[mesh_group->weights_offset + morph_target] = weight;
                                                }
    
                                                break;
                                            }
                                        }
    
                                        break;
                                    }
                                    case BinaryAnimation::InterpolationType::Step: {
                                        switch (channel.path) {
                                            case BinaryAnimation::PathType::Position: {
                                                entity.set_local_position(f32vec3_values[i]);
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Rotation: {
                                                f32vec4 value_1 = f32vec4_values[i];
                                                glm::quat quat = glm::quat(value_1.w, value_1.x, value_1.y, value_1.z);
                                                entity.set_local_rotation(glm::degrees(glm::eulerAngles(quat)));
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Scale: {
                                                entity.set_local_scale(f32vec3_values[i]);
                                                break;
                                            }
                                            case BinaryAnimation::PathType::Weights: {
                                                const u32 morph_target_offset = s_cast<u32>(mesh_group->weights.size());
    
                                                for(u32 morph_target = 0; morph_target < mesh_group->weights.size(); morph_target++) {
                                                    f32 weight = f32_values[morph_target_offset * i + morph_target];

                                                    calculated_weights[mesh_group->weights_offset + morph_target] = weight;
                                                }
                                                
                                                break;
                                            }
                                        }
    
                                        break;
                                    }
                                    case BinaryAnimation::InterpolationType::CubicSpline: {
                                        throw std::runtime_error("something went wrong");
                                        break;
                                    }
                                }
    
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    auto AssetManager::record_manifest_update(const RecordManifestUpdateInfo& info) -> daxa::ExecutableCommandList {
        PROFILE_SCOPE;
        auto cmd_recorder = context->device.create_command_recorder({ .name = "asset manager update" });

        reallocate_buffer(context, cmd_recorder, gpu_meshes, s_cast<u32>(total_mesh_count * sizeof(Mesh)));

        reallocate_buffer<AnimatedMeshData>(context, cmd_recorder, gpu_animated_meshes, s_cast<u32>(total_animated_mesh_count * sizeof(AnimatedMesh) + sizeof(AnimatedMeshData)), [&](const daxa::BufferId& buffer) -> AnimatedMeshData {
            return AnimatedMeshData {
                .count = s_cast<u32>(total_animated_mesh_count),
                .animated_meshes = context->device.buffer_device_address(buffer).value() + sizeof(AnimatedMeshData)
            };
        });
        reallocate_buffer<PrefixSumWorkExpansion>(context, cmd_recorder, gpu_animated_mesh_vertices_prefix_sum_work_expansion, total_animated_mesh_count * sizeof(u32) * 3 + sizeof(PrefixSumWorkExpansion), [&](const daxa::BufferId& buffer) -> PrefixSumWorkExpansion {
            return PrefixSumWorkExpansion {
                .merged_expansion_count_thread_count = 0,
                .expansions_max = s_cast<u32>(total_animated_mesh_count),
                .workgroup_count = 0,
                .expansions_inclusive_prefix_sum = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_animated_mesh_count * sizeof(u32) * 0,
                .expansions_src_work_item = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_animated_mesh_count * sizeof(u32) * 1,
                .expansions_expansion_factor = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_animated_mesh_count * sizeof(u32) * 2
            };
        });
        reallocate_buffer<PrefixSumWorkExpansion>(context, cmd_recorder, gpu_animated_mesh_meshlets_prefix_sum_work_expansion, total_animated_mesh_count * sizeof(u32) * 3 + sizeof(PrefixSumWorkExpansion), [&](const daxa::BufferId& buffer) -> PrefixSumWorkExpansion {
            return PrefixSumWorkExpansion {
                .merged_expansion_count_thread_count = 0,
                .expansions_max = s_cast<u32>(total_animated_mesh_count),
                .workgroup_count = 0,
                .expansions_inclusive_prefix_sum = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_animated_mesh_count * sizeof(u32) * 0,
                .expansions_src_work_item = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_animated_mesh_count * sizeof(u32) * 1,
                .expansions_expansion_factor = context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_animated_mesh_count * sizeof(u32) * 2
            };
        });
        reallocate_buffer(context, cmd_recorder, gpu_calculated_weights, s_cast<u32>(calculated_weights.size() * sizeof(f32)));

        reallocate_buffer(context, cmd_recorder, gpu_materials, s_cast<u32>(material_manifest_entries.size() * sizeof(Material)));
        reallocate_buffer(context, cmd_recorder, gpu_mesh_groups, s_cast<u32>(total_mesh_group_count * sizeof(MeshGroup)));
        reallocate_buffer(context, cmd_recorder, gpu_mesh_indices, s_cast<u32>(total_mesh_count * sizeof(u32)));
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

        const u32 readback_mesh_size = round_up_div(s_cast<u32>(total_mesh_count), 32u);

        readback_material.resize(material_manifest_entries.size());
        texture_sizes.resize(texture_manifest_entries.size());
        readback_mesh.resize(readback_mesh_size);
        reallocate_buffer(context, cmd_recorder, gpu_readback_material_gpu, s_cast<u32>(material_manifest_entries.size() * sizeof(u32)));
        reallocate_buffer(context, cmd_recorder, gpu_readback_material_cpu, s_cast<u32>(material_manifest_entries.size() * sizeof(u32)));
        reallocate_buffer(context, cmd_recorder, gpu_readback_mesh_gpu, readback_mesh_size * sizeof(u32));
        reallocate_buffer(context, cmd_recorder, gpu_readback_mesh_cpu, readback_mesh_size * sizeof(u32));

        if(!dirty_meshes.empty()) {
            daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer meshes"
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            
            Mesh mesh = {};
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

        if(!dirty_animated_meshes.empty()) {
            daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = sizeof(AnimatedMesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer animated meshes"
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);

            AnimatedMesh animated_mesh = {};
            std::memcpy(context->device.buffer_host_address(staging_buffer).value(), &animated_mesh, sizeof(AnimatedMesh));

            for(const u32 animated_mesh_index : dirty_animated_meshes) {
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_animated_meshes.get_state().buffers[0],
                    .src_offset = 0,
                    .dst_offset = sizeof(AnimatedMeshData) + animated_mesh_index * sizeof(AnimatedMesh),
                    .size = sizeof(AnimatedMesh)
                });
            }
            
            dirty_animated_meshes = {};
        }

        if(!dirty_mesh_groups.empty()) {
            std::vector<MeshGroup> mesh_groups = {};
            std::vector<u32> meshes = {};
            u64 buffer_ptr = context->device.buffer_device_address(gpu_mesh_indices.get_state().buffers[0]).value();
            mesh_groups.reserve(dirty_mesh_groups.size());
            for(const auto& mesh_group_mapping : dirty_mesh_groups) {
                const auto& mesh_group = mesh_group_manifest_entries[mesh_group_mapping.mesh_group_manifest_index];
                
                mesh_groups.push_back(MeshGroup {
                    .mesh_indices = buffer_ptr + mesh_group_mapping.mesh_offset * sizeof(u32),
                    .count = mesh_group.mesh_count,
                    .padding = {},
                });
                
                meshes.reserve(meshes.size() + mesh_group.mesh_count);
                for(u32 i = 0; i < mesh_group.mesh_count; i++) {
                    meshes.push_back(mesh_group_mapping.mesh_offset + i);
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
            for(const auto& mesh_group_mapping : dirty_mesh_groups) {
                const auto& mesh_group = mesh_group_manifest_entries[mesh_group_mapping.mesh_group_manifest_index];

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_mesh_groups.get_state().buffers[0],
                    .src_offset = mesh_group_offset,
                    .dst_offset = mesh_group_mapping.mesh_group_index * sizeof(MeshGroup),
                    .size = sizeof(MeshGroup)
                });

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_mesh_indices.get_state().buffers[0],
                    .src_offset = meshes_offset,
                    .dst_offset = mesh_group_mapping.mesh_offset * sizeof(u32),
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

                mesh_manifest.geometry_info = MeshManifestEntry::VirtualGeometryRenderInfo { .mesh_geometry_data = mesh_upload_info.mesh_geometry_data, };
                if(!mesh_manifest.geometry_info.has_value() && binary_mesh.material_index.has_value()) {
                    u32 material_index = mesh_upload_info.material_manifest_offset + binary_mesh.material_index.value();
                    auto& material_manifest = material_manifest_entries.at(material_index);

                    cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                        .src_buffer = material_null_buffer,
                        .dst_buffer = gpu_materials.get_state().buffers[0],
                        .src_offset = 0,
                        .dst_offset = material_manifest.material_manifest_index * sizeof(Material),
                        .size = sizeof(Material),
                    });
                    mesh_manifest.geometry_info->material_manifest_index = material_manifest.material_manifest_index;
                }
            }

            daxa::BufferId staging_buffer = context->create_buffer({
                .size = info.uploaded_meshes.size() * sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "mesh manifest upload staging buffer",
            });

            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            Mesh* staging_ptr = context->device.buffer_host_address_as<Mesh>(staging_buffer).value();

            for (u32 upload_index = 0; upload_index < info.uploaded_meshes.size(); upload_index++) {
                auto const & upload = info.uploaded_meshes[upload_index];
                MeshManifestEntry& mesh_manifest_entry = mesh_manifest_entries[upload.manifest_index];
                AssetManifestEntry& asset_manifest_entry = asset_manifest_entries[mesh_manifest_entry.asset_manifest_index];
                u32 mesh_group_index = asset_manifest_entry.mesh_group_manifest_offset + mesh_manifest_entry.asset_local_mesh_index;
                MeshGroupManifestEntry& mesh_group_manifest_entry = mesh_group_manifest_entries[mesh_group_index];

                if(!mesh_group_manifest_entry.has_morph_targets) {
                    Mesh mesh = {
                        .material_index = upload.mesh_geometry_data.material_index,
                        .meshlet_count = upload.mesh_geometry_data.meshlet_count,
                        .vertex_count = upload.mesh_geometry_data.vertex_count,
                        .aabb = upload.mesh_geometry_data.aabb,
                        .meshlets = upload.mesh_geometry_data.meshlets,
                        .bounding_spheres = upload.mesh_geometry_data.bounding_spheres,
                        .simplification_errors = upload.mesh_geometry_data.simplification_errors,
                        .meshlet_aabbs = upload.mesh_geometry_data.meshlet_aabbs,
                        .micro_indices = upload.mesh_geometry_data.micro_indices,
                        .indirect_vertices = upload.mesh_geometry_data.indirect_vertices,
                        .primitive_indices = upload.mesh_geometry_data.primitive_indices,
                        .vertex_positions = upload.mesh_geometry_data.vertex_positions,
                        .vertex_normals = upload.mesh_geometry_data.vertex_normals,
                        .vertex_uvs = upload.mesh_geometry_data.vertex_uvs,
                    };
    
                    std::memcpy(staging_ptr + upload_index, &mesh, sizeof(Mesh));
    
                    for(u32 mesh_index : mesh_manifest_entry.mesh_indices) {
                        cmd_recorder.copy_buffer_to_buffer({
                            .src_buffer = staging_buffer,
                            .dst_buffer = gpu_meshes.get_state().buffers[0],
                            .src_offset = upload_index * sizeof(Mesh),
                            .dst_offset = mesh_index * sizeof(Mesh),
                            .size = sizeof(Mesh),
                        });
                    }
                } else {
                    daxa::DeviceAddress calculated_weights_address = context->device.buffer_device_address(gpu_calculated_weights.get_state().buffers[0]).value();

                    daxa::BufferId staging_mesh_buffer = context->create_buffer({
                        .size = mesh_manifest_entry.mesh_indices.size() * sizeof(Mesh),
                        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                        .name = "mesh 2 manifest upload staging buffer",
                    });

                    context->destroy_buffer_deferred(cmd_recorder, staging_mesh_buffer);
                    Mesh* staging_mesh_ptr = context->device.buffer_host_address_as<Mesh>(staging_mesh_buffer).value();

                    daxa::BufferId staging_animated_mesh_buffer = context->create_buffer({
                        .size = mesh_manifest_entry.animated_mesh_indices.size() * sizeof(AnimatedMesh),
                        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                        .name = "animated mesh manifest upload staging buffer",
                    });

                    context->destroy_buffer_deferred(cmd_recorder, staging_animated_mesh_buffer);
                    AnimatedMesh* staging_animated_mesh_ptr = context->device.buffer_host_address_as<AnimatedMesh>(staging_animated_mesh_buffer).value();

                    for(u32 i = 0; i < mesh_manifest_entry.animated_mesh_indices.size(); i++) {
                        u32 mesh_index = mesh_manifest_entry.mesh_indices[i];
                        u32 animated_mesh_index = mesh_manifest_entry.animated_mesh_indices[i].animated_mesh_index;

                        u64 total_animated_mesh_buffer_size = {};
                        total_animated_mesh_buffer_size += upload.mesh_geometry_data.meshlet_count * sizeof(AABB);
                        total_animated_mesh_buffer_size += upload.mesh_geometry_data.meshlet_count * sizeof(MeshletBoundingSpheres);
                        total_animated_mesh_buffer_size += upload.mesh_geometry_data.vertex_count * sizeof(f32vec3);
                        total_animated_mesh_buffer_size += upload.mesh_geometry_data.vertex_count * sizeof(u32);
                        total_animated_mesh_buffer_size += upload.mesh_geometry_data.vertex_count * sizeof(u32);
    
                        daxa::BufferId animated_mesh_buffer = context->create_buffer(daxa::BufferInfo {
                            .size = s_cast<daxa::usize>(total_animated_mesh_buffer_size),
                            .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                            .name = "animated mesh buffer: " + asset_manifest_entry.path.filename().string() + " mesh " + std::to_string(mesh_group_index) + " primitive " + std::to_string(upload.manifest_index) + " " + std::to_string(i)
                        });

                        mesh_manifest_entry.animated_mesh_indices[i].animated_mesh_buffer = animated_mesh_buffer;
    
                        daxa::DeviceAddress aabbs_address = context->device.buffer_device_address(animated_mesh_buffer).value();
                        daxa::DeviceAddress bounding_spheres_address = aabbs_address + upload.mesh_geometry_data.meshlet_count * sizeof(AABB);
                        daxa::DeviceAddress positions_address = bounding_spheres_address + upload.mesh_geometry_data.meshlet_count * sizeof(MeshletBoundingSpheres);
                        daxa::DeviceAddress normals_address = positions_address + upload.mesh_geometry_data.vertex_count * sizeof(f32vec3);
                        daxa::DeviceAddress uvs_address = normals_address + upload.mesh_geometry_data.vertex_count * sizeof(u32);
    
                        AnimatedMesh animated_mesh = {
                            .meshlet_count = upload.mesh_geometry_data.meshlet_count,
                            .vertex_count = upload.mesh_geometry_data.vertex_count,
                            .aabb = upload.mesh_geometry_data.aabb,
                            .meshlets = upload.mesh_geometry_data.meshlets,
                            .bounding_spheres = bounding_spheres_address,
                            .simplification_errors = upload.mesh_geometry_data.simplification_errors,
                            .meshlet_aabbs = aabbs_address,
                            .micro_indices = upload.mesh_geometry_data.micro_indices,
                            .indirect_vertices = upload.mesh_geometry_data.indirect_vertices,
                            .primitive_indices = upload.mesh_geometry_data.primitive_indices,
                            .original_vertex_positions = upload.mesh_geometry_data.vertex_positions,
                            .original_vertex_normals = upload.mesh_geometry_data.vertex_normals,
                            .original_vertex_uvs = upload.mesh_geometry_data.vertex_uvs,
                            .morph_target_position_count = upload.mesh_geometry_data.morph_target_position_count,
                            .morph_target_normal_count = upload.mesh_geometry_data.morph_target_normal_count,
                            .morph_target_uv_count = upload.mesh_geometry_data.morph_target_uv_count,
                            .morph_target_positions = upload.mesh_geometry_data.morph_target_positions,
                            .morph_target_normals = upload.mesh_geometry_data.morph_target_normals,
                            .morph_target_uvs = upload.mesh_geometry_data.morph_target_uvs,
                            // .morph_target_positions = upload.mesh_geometry_data.vertex_positions,
                            // .morph_target_normals = upload.mesh_geometry_data.vertex_normals,
                            // .morph_target_uvs = upload.mesh_geometry_data.vertex_uvs,
                            .calculated_weights = calculated_weights_address + mesh_group_manifest_entry.weights_offset,
                            .vertex_positions = positions_address,
                            .vertex_normals = normals_address,
                            .vertex_uvs = uvs_address,
                        };

                        std::memcpy(staging_animated_mesh_ptr + i, &animated_mesh, sizeof(AnimatedMesh));

                        cmd_recorder.copy_buffer_to_buffer({
                            .src_buffer = staging_animated_mesh_buffer,
                            .dst_buffer = gpu_animated_meshes.get_state().buffers[0],
                            .src_offset = i * sizeof(AnimatedMesh),
                            .dst_offset = sizeof(AnimatedMeshData) + animated_mesh_index * sizeof(AnimatedMesh),
                            .size = sizeof(AnimatedMesh),
                        });
    
                        Mesh mesh = {
                            .material_index = upload.mesh_geometry_data.material_index,
                            .meshlet_count = upload.mesh_geometry_data.meshlet_count,
                            .vertex_count = upload.mesh_geometry_data.vertex_count,
                            .aabb = upload.mesh_geometry_data.aabb,
                            .meshlets = upload.mesh_geometry_data.meshlets,
                            .bounding_spheres = bounding_spheres_address,
                            .simplification_errors = upload.mesh_geometry_data.simplification_errors,
                            .meshlet_aabbs = aabbs_address,
                            .micro_indices = upload.mesh_geometry_data.micro_indices,
                            .indirect_vertices = upload.mesh_geometry_data.indirect_vertices,
                            .primitive_indices = upload.mesh_geometry_data.primitive_indices,
                            .vertex_positions = positions_address,
                            .vertex_normals = normals_address,
                            .vertex_uvs = uvs_address,
                        };
    
                        std::memcpy(staging_mesh_ptr + i, &mesh, sizeof(Mesh));

                        cmd_recorder.copy_buffer_to_buffer({
                            .src_buffer = staging_mesh_buffer,
                            .dst_buffer = gpu_meshes.get_state().buffers[0],
                            .src_offset = i * sizeof(Mesh),
                            .dst_offset = mesh_index * sizeof(Mesh),
                            .size = sizeof(Mesh),
                        });
                    }
                }
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
            dirty_material_manifest_indices.push_back(mesh_upload_info.mesh_geometry_data.material_index);
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

        if(!calculated_weights.empty()) {
            daxa::BufferId staging_calculated_weights_buffer = context->create_buffer({
                .size = calculated_weights.size() * sizeof(f32),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging calculated weights buffer",
            });
    
            context->destroy_buffer_deferred(cmd_recorder, staging_calculated_weights_buffer);
    
            std::memcpy(context->device.buffer_host_address(staging_calculated_weights_buffer).value(), calculated_weights.data(), calculated_weights.size() * sizeof(f32));
    
            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = staging_calculated_weights_buffer,
                .dst_buffer = gpu_calculated_weights.get_state().buffers[0],
                .size = calculated_weights.size() * sizeof(f32),
            });
        }

        cmd_recorder.pipeline_barrier({
            .src_access = daxa::AccessConsts::TRANSFER_WRITE,
            .dst_access = daxa::AccessConsts::READ_WRITE,
        });

        return cmd_recorder.complete_current_commands();
    }
}