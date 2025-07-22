#include "gpu_asset_manager_data.hpp"

#include "asset_manager.hpp"
#include <graphics/helper.hpp>

namespace foundation {
    GPUAssetManagerData::GPUAssetManagerData(Context* _context, AssetManager* _asset_manager) : context{_context}, asset_manager{_asset_manager} {
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

    GPUAssetManagerData::~GPUAssetManagerData() {
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
    }


#pragma region UPDATE
    void GPUAssetManagerData::update(daxa::CommandRecorder& cmd_recorder, const std::span<const MeshUploadInfo>& uploaded_meshes, const std::span<const TextureUploadInfo>& uploaded_textures) {
        update_buffers(cmd_recorder);
        update_dirty_data(cmd_recorder);
        update_meshes(cmd_recorder, uploaded_meshes);
        update_materials(cmd_recorder, uploaded_meshes, uploaded_textures);
        update_animation_data(cmd_recorder);
    }
#pragma endregion

#pragma region UPDATE BUFFERS
    void GPUAssetManagerData::update_buffers(daxa::CommandRecorder& cmd_recorder) {
        usize total_mesh_count = asset_manager->total_mesh_count;
        usize total_animated_mesh_count = asset_manager->total_animated_mesh_count;

        usize total_opaque_mesh_count = asset_manager->total_opaque_mesh_count;
        usize total_masked_mesh_count = asset_manager->total_masked_mesh_count;
        usize total_transparent_mesh_count = asset_manager->total_transparent_mesh_count;

        usize total_meshlet_count = asset_manager->total_meshlet_count;
        usize total_opaque_meshlet_count = asset_manager->total_opaque_meshlet_count;
        usize total_masked_meshlet_count = asset_manager->total_masked_meshlet_count;
        usize total_transparent_meshlet_count = asset_manager->total_transparent_meshlet_count;

        usize total_material_count = asset_manager->material_manifest_entries.size();
        usize total_texture_count = asset_manager->texture_manifest_entries.size();

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
        reallocate_buffer(context, cmd_recorder, gpu_calculated_weights, s_cast<u32>(asset_manager->calculated_weights.size() * sizeof(f32)));

        reallocate_buffer(context, cmd_recorder, gpu_materials, s_cast<u32>(total_material_count * sizeof(Material)));
        reallocate_buffer(context, cmd_recorder, gpu_mesh_groups, s_cast<u32>(asset_manager->total_mesh_group_count * sizeof(MeshGroup)));
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

        asset_manager->readback_material.resize(total_material_count);
        asset_manager->texture_sizes.resize(total_texture_count);
        asset_manager->readback_mesh.resize(readback_mesh_size);
        reallocate_buffer(context, cmd_recorder, gpu_readback_material_gpu, s_cast<u32>(total_material_count * sizeof(u32)));
        reallocate_buffer(context, cmd_recorder, gpu_readback_material_cpu, s_cast<u32>(total_material_count * sizeof(u32)));
        reallocate_buffer(context, cmd_recorder, gpu_readback_mesh_gpu, readback_mesh_size * sizeof(u32));
        reallocate_buffer(context, cmd_recorder, gpu_readback_mesh_cpu, readback_mesh_size * sizeof(u32));
    }
#pragma endregion


#pragma region UPDATE DIRTY DATA
    void GPUAssetManagerData::update_dirty_data(daxa::CommandRecorder& cmd_recorder) {
        auto& dirty_meshes = asset_manager->dirty_meshes;
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

        auto& dirty_animated_meshes = asset_manager->dirty_animated_meshes;
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

        auto& dirty_mesh_groups = asset_manager->dirty_mesh_groups;
        auto& mesh_group_manifest_entries = asset_manager->mesh_group_manifest_entries;

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
                .name = "staging buffer mesh group data"
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

        auto& dirty_materials = asset_manager->dirty_materials;
        if(!dirty_materials.empty()) {
            daxa::BufferId staging_buffer = context->create_buffer(daxa::BufferInfo {
                .size = sizeof(Material),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer material"
            });
            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);

            Material material = {};
            std::memcpy(context->device.buffer_host_address(staging_buffer).value(), &material, sizeof(Material));

            for(const u32 material_index : dirty_materials) {
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_materials.get_state().buffers[0],
                    .src_offset = 0,
                    .dst_offset = material_index * sizeof(Material),
                    .size = sizeof(Material)
                });
            }
            
            dirty_materials = {};
        }
    }
#pragma endregion

#pragma region UPDATE MESHES
    void GPUAssetManagerData::update_meshes(daxa::CommandRecorder& cmd_recorder, const std::span<const MeshUploadInfo>& uploaded_meshes) {
        auto& asset_manifest_entries = asset_manager->asset_manifest_entries;
        auto& mesh_group_manifest_entries = asset_manager->mesh_group_manifest_entries;
        auto& mesh_manifest_entries = asset_manager->mesh_manifest_entries;
        
        if(!uploaded_meshes.empty()) {
            daxa::BufferId staging_buffer = context->create_buffer({
                .size = uploaded_meshes.size() * sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "mesh manifest upload staging buffer",
            });

            context->destroy_buffer_deferred(cmd_recorder, staging_buffer);
            Mesh* staging_ptr = context->device.buffer_host_address_as<Mesh>(staging_buffer).value();

            for (u32 upload_index = 0; upload_index < uploaded_meshes.size(); upload_index++) {
                auto const & upload = uploaded_meshes[upload_index];
                MeshManifestEntry& mesh_manifest_entry = mesh_manifest_entries[upload.manifest_index];
                AssetManifestEntry& asset_manifest_entry = asset_manifest_entries[mesh_manifest_entry.asset_manifest_index];
                u32 mesh_group_index = asset_manifest_entry.mesh_group_manifest_offset + mesh_manifest_entry.asset_local_mesh_index;
                MeshGroupManifestEntry& mesh_group_manifest_entry = mesh_group_manifest_entries[mesh_group_index];

                auto& mesh_manifest = mesh_manifest_entries[upload.manifest_index];
                mesh_manifest.loading = false;
                mesh_manifest.geometry_info = MeshManifestEntry::VirtualGeometryRenderInfo { .mesh_geometry_data = upload.mesh_geometry_data };

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
    }
#pragma endregion

#pragma region UPDATE MATERIALS
    void GPUAssetManagerData::update_materials(daxa::CommandRecorder& cmd_recorder, const std::span<const MeshUploadInfo>& uploaded_meshes, const std::span<const TextureUploadInfo>& uploaded_textures) {
        auto& texture_manifest_entries = asset_manager->texture_manifest_entries;
        auto& material_manifest_entries = asset_manager->material_manifest_entries;

        std::vector<u32> dirty_material_manifest_indices = {};
        for(const auto& texture_upload_info : uploaded_textures) {
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
        
        for(const auto& mesh_upload_info : uploaded_meshes) {
            if(mesh_upload_info.mesh_geometry_data.material_index == INVALID_ID) { continue; }
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
    }
#pragma endregion

#pragma region UPDATE ANIMATION DATA
    void GPUAssetManagerData::update_animation_data(daxa::CommandRecorder& cmd_recorder) {
        auto& calculated_weights = asset_manager->calculated_weights;
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
    }
#pragma endregion
}