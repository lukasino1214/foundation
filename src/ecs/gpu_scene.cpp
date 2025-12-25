#include "gpu_scene.hpp"
#include <graphics/helper.hpp>

namespace foundation {
    GPUScene::GPUScene(Context* _context) : context{_context} {
        gpu_meshes = make_task_buffer(context, {
            .size = sizeof(Mesh),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "meshes"
        });

        gpu_mesh_groups = make_task_buffer(context, {
            .size = sizeof(MeshGroup),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "mesh groups"
        });

        gpu_mesh_indices = make_task_buffer(context, {
            .size = sizeof(u32),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "mesh indices"
        });
        
        gpu_transforms = make_task_buffer(context, {
            .size = sizeof(f32mat4x3),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "transforms"
        });

        gpu_culled_meshes_data = make_task_buffer(context, {
            .size = sizeof(MeshesData),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "culled meshes data"
        });

        gpu_opaque_prefix_sum_work_expansion_mesh = make_task_buffer(context, {
            .size = sizeof(PrefixSumWorkExpansion),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "opaque prefix sum work expansion mesh"
        });

        gpu_masked_prefix_sum_work_expansion_mesh = make_task_buffer(context, {
            .size = sizeof(PrefixSumWorkExpansion),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "masked prefix sum work expansion mesh"
        });

        gpu_transparent_prefix_sum_work_expansion_mesh = make_task_buffer(context, {
            .size = sizeof(PrefixSumWorkExpansion),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "transparent prefix sum work expansion mesh"
        });

        gpu_meshlets_instance_data = make_task_buffer(context, {
            .size = sizeof(MeshletInstanceData),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "meshlets instance data"
        });

        gpu_meshlets_data_merged_opaque = make_task_buffer(context, {
            .size = sizeof(MeshletsDataMerged),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "meshlets data merged opaque"
        });

        gpu_meshlets_data_merged_masked = make_task_buffer(context, {
            .size = sizeof(MeshletsDataMerged),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "meshlets data merged masked"
        });

        gpu_meshlets_data_merged_transparent = make_task_buffer(context, {
            .size = sizeof(MeshletsDataMerged),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "meshlets data merged transparent"
        });
    }

    GPUScene::~GPUScene() {
        context->device.destroy_buffer(gpu_meshes.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_mesh_groups.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_mesh_indices.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_transforms.get_state().buffers[0]);
        
        context->device.destroy_buffer(gpu_culled_meshes_data.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_opaque_prefix_sum_work_expansion_mesh.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_masked_prefix_sum_work_expansion_mesh.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_transparent_prefix_sum_work_expansion_mesh.get_state().buffers[0]);
        
        context->device.destroy_buffer(gpu_meshlets_instance_data.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_meshlets_data_merged_opaque.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_meshlets_data_merged_masked.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_meshlets_data_merged_transparent.get_state().buffers[0]);
    }

    void GPUScene::update(daxa::CommandRecorder& cmd_recorder, const UpdateInfo& info) {
        PROFILE_SCOPE;

        total_mesh_group_count = info.total_mesh_group_count;
        total_mesh_count = info.total_mesh_count;
        total_opaque_mesh_count = info.total_opaque_mesh_count;
        total_masked_mesh_count = info.total_masked_mesh_count;
        total_transparent_mesh_count = info.total_transparent_mesh_count;
        total_meshlet_count = info.total_meshlet_count;
        total_opaque_meshlet_count = info.total_opaque_meshlet_count;
        total_masked_meshlet_count = info.total_masked_meshlet_count;
        total_transparent_meshlet_count = info.total_transparent_meshlet_count;

        auto dirty_meshes = info.dirty_meshes;
        auto dirty_mesh_groups = info.dirty_mesh_groups;

        reallocate_buffer(context, cmd_recorder, gpu_meshes, s_cast<u32>(total_mesh_count * sizeof(Mesh)));
        reallocate_buffer(context, cmd_recorder, gpu_mesh_groups, s_cast<u32>(total_mesh_group_count * sizeof(MeshGroup)));
        reallocate_buffer(context, cmd_recorder, gpu_transforms, s_cast<u32>(total_mesh_group_count * sizeof(f32mat4x3)));
        reallocate_buffer(context, cmd_recorder, gpu_mesh_indices, s_cast<u32>(total_mesh_count * sizeof(u32)));
        reallocate_buffer<MeshesData>(context, cmd_recorder, gpu_culled_meshes_data, s_cast<u32>(total_mesh_count * sizeof(MeshData) + sizeof(MeshesData)), [&](const daxa::BufferId& buffer) -> MeshesData {
            return MeshesData {
                .count = 0,
                .meshes = context->device.buffer_device_address(buffer).value() + sizeof(MeshesData)
            };
        });

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

        if(reallocate_buffer(context, cmd_recorder, gpu_meshlets_instance_data, 2 * total_meshlet_count * sizeof(MeshletInstanceData))) {
            daxa::DeviceAddress buffer_address = context->device.buffer_device_address(gpu_meshlets_instance_data.get_state().buffers[0]).value();
            
            u32 hw_offset = 0;
            u32 sw_offset = s_cast<u32>(total_opaque_meshlet_count);
            {
                daxa::BufferId stagging_buffer = context->device.create_buffer(daxa::BufferInfo {
                    .size = sizeof(MeshletsDataMerged),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "opaque stagging buffer"
                });
                cmd_recorder.destroy_buffer_deferred(stagging_buffer);

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
                daxa::BufferId stagging_buffer = context->device.create_buffer(daxa::BufferInfo {
                    .size = sizeof(MeshletsDataMerged),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "masked stagging buffer"
                });
                cmd_recorder.destroy_buffer_deferred(stagging_buffer);

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
                daxa::BufferId stagging_buffer = context->device.create_buffer(daxa::BufferInfo {
                    .size = sizeof(MeshletsDataMerged),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "transparent stagging buffer"
                });
                cmd_recorder.destroy_buffer_deferred(stagging_buffer);

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

        if(!dirty_meshes.empty()) {
            daxa::BufferId staging_buffer = context->device.create_buffer(daxa::BufferInfo {
                .size = sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer meshes"
            });
            cmd_recorder.destroy_buffer_deferred(staging_buffer);

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

        if(!dirty_mesh_groups.empty()) {
            std::vector<MeshGroup> mesh_groups = {};
            std::vector<u32> meshes = {};
            u64 buffer_ptr = context->device.buffer_device_address(gpu_mesh_indices.get_state().buffers[0]).value();
            mesh_groups.reserve(dirty_mesh_groups.size());
            for(const auto& mesh_group_mapping : dirty_mesh_groups) {
                mesh_groups.push_back(MeshGroup {
                    .mesh_indices = buffer_ptr + mesh_group_mapping.mesh_offset * sizeof(u32),
                    .count = mesh_group_mapping.mesh_count,
                    .padding = {},
                });
                
                meshes.reserve(meshes.size() + mesh_group_mapping.mesh_count);
                for(u32 i = 0; i < mesh_group_mapping.mesh_count; i++) {
                    meshes.push_back(mesh_group_mapping.mesh_offset + i);
                }
            }

            usize staging_size = mesh_groups.size() * sizeof(MeshGroup) + meshes.size() * sizeof(u32);
            daxa::BufferId staging_buffer = context->device.create_buffer(daxa::BufferInfo {
                .size = staging_size,
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging buffer scene data"
            });
            cmd_recorder.destroy_buffer_deferred(staging_buffer);
            std::byte* ptr = context->device.buffer_host_address(staging_buffer).value();

            usize offset = 0; 
            std::memcpy(ptr + offset, mesh_groups.data(), mesh_groups.size() * sizeof(MeshGroup));
            offset += mesh_groups.size() * sizeof(MeshGroup);
            std::memcpy(ptr + offset, meshes.data(), meshes.size() * sizeof(u32));
            offset += meshes.size() * sizeof(u32);

            usize mesh_group_offset = 0;
            usize meshes_offset = mesh_group_offset + mesh_groups.size() * sizeof(MeshGroup);
            for(const auto& mesh_group_mapping : dirty_mesh_groups) {
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
                    .size = mesh_group_mapping.mesh_count * sizeof(u32)
                });

                mesh_group_offset += sizeof(MeshGroup);
                meshes_offset += mesh_group_mapping.mesh_count * sizeof(u32);
            }
            dirty_mesh_groups.clear();
        }

        // if(!info.uploaded_meshes.empty()) {
        //     daxa::BufferId material_null_buffer = context->device.create_buffer({
        //         .size = s_cast<u32>(sizeof(Material)),
        //         .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        //         .name = "material null buffer",
        //     });
        //     cmd_recorder.destroy_buffer_deferred(material_null_buffer);
        //     {
        //         Material material = {};
        //         material.albedo_factor = f32vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
        //         material.metallic_factor = 1.0f;
        //         material.roughness_factor = 1.0f;
        //         material.emissive_factor = { 0.0f, 0.0f, 0.0f };
        //         material.alpha_mode = 0;
        //         material.alpha_cutoff = 0.5f;
        //         material.double_sided = 0u;

        //         std::memcpy(context->device.buffer_host_address_as<Material>(material_null_buffer).value(), &material, sizeof(Material));
        //     }

        //     for(const auto& mesh_upload_info : info.uploaded_meshes) {
        //         auto& mesh_manifest = mesh_manifest_entries[mesh_upload_info.manifest_index];
        //         mesh_manifest.loading = false;
        //         auto& asset_manifest = asset_manifest_entries[mesh_manifest.asset_manifest_index];
        //         const BinaryMesh& binary_mesh = asset_manifest.asset->meshes[asset_manifest.asset->mesh_groups[mesh_manifest.asset_local_mesh_index].mesh_offset + mesh_manifest.asset_local_primitive_index];

        //         mesh_manifest.geometry_info = MeshManifestEntry::VirtualGeometryRenderInfo { .mesh_geometry_data = mesh_upload_info.mesh_geometry_data, };
        //         if(!mesh_manifest.geometry_info.has_value() && binary_mesh.material_index.has_value()) {
        //             u32 material_index = mesh_upload_info.material_manifest_offset + binary_mesh.material_index.value();
        //             auto& material_manifest = material_manifest_entries.at(material_index);

        //             cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
        //                 .src_buffer = material_null_buffer,
        //                 .dst_buffer = gpu_materials.get_state().buffers[0],
        //                 .src_offset = 0,
        //                 .dst_offset = material_manifest.material_manifest_index * sizeof(Material),
        //                 .size = sizeof(Material),
        //             });
        //             mesh_manifest.geometry_info->material_manifest_index = material_manifest.material_manifest_index;
        //         }
        //     }

        //     u32 total_gpu_mesh_count = {};
        //     for (u32 upload_index = 0; upload_index < info.uploaded_meshes.size(); upload_index++) {
        //         const auto& upload = info.uploaded_meshes[upload_index];
            
        //         MeshManifestEntry& mesh_manifest_entry = mesh_manifest_entries[upload.manifest_index];
        //         total_gpu_mesh_count += mesh_manifest_entry.gpu_mesh_indices.size();
        //     }

        //     daxa::BufferId staging_buffer = context->device.create_buffer({
        //         .size = total_gpu_mesh_count * sizeof(Mesh),
        //         .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        //         .name = "mesh manifest upload staging buffer",
        //     });

        //     cmd_recorder.destroy_buffer_deferred(staging_buffer);
        //     Mesh* staging_ptr = context->device.buffer_host_address_as<Mesh>(staging_buffer).value();

        //     u32 gpu_mesh_index_iter = 0;
        //     for (u32 upload_index = 0; upload_index < info.uploaded_meshes.size(); upload_index++) {
        //         const auto& upload = info.uploaded_meshes[upload_index];
        //         MeshManifestEntry& mesh_manifest_entry = mesh_manifest_entries[upload.manifest_index];
                
        //         for(auto [gpu_mesh_group_index, gpu_mesh_index] : mesh_manifest_entry.gpu_mesh_indices) {
        //             Mesh mesh = {
        //                 .mesh_group_index = gpu_mesh_group_index,
        //                 .manifest_index = upload.mesh_geometry_data.manifest_index,
        //                 .material_index = upload.mesh_geometry_data.material_index,
        //                 .meshlet_count = upload.mesh_geometry_data.meshlet_count,
        //                 .vertex_count = upload.mesh_geometry_data.vertex_count,
        //                 .aabb = upload.mesh_geometry_data.aabb,
        //                 .meshlets = upload.mesh_geometry_data.meshlets,
        //                 .bounding_spheres = upload.mesh_geometry_data.bounding_spheres,
        //                 .simplification_errors = upload.mesh_geometry_data.simplification_errors,
        //                 .meshlet_aabbs = upload.mesh_geometry_data.meshlet_aabbs,
        //                 .micro_indices = upload.mesh_geometry_data.micro_indices,
        //                 .indirect_vertices = upload.mesh_geometry_data.indirect_vertices,
        //                 .primitive_indices = upload.mesh_geometry_data.primitive_indices,
        //                 .vertex_positions = upload.mesh_geometry_data.vertex_positions,
        //                 .vertex_normals = upload.mesh_geometry_data.vertex_normals,
        //                 .vertex_uvs = upload.mesh_geometry_data.vertex_uvs,
        //             };
        //             std::memcpy(staging_ptr + gpu_mesh_index_iter, &mesh, sizeof(Mesh));

        //             cmd_recorder.copy_buffer_to_buffer({
        //                 .src_buffer = staging_buffer,
        //                 .dst_buffer = gpu_meshes.get_state().buffers[0],
        //                 .src_offset = gpu_mesh_index_iter * sizeof(Mesh),
        //                 .dst_offset = gpu_mesh_index * sizeof(Mesh),
        //                 .size = sizeof(Mesh),
        //             });

        //             gpu_mesh_index_iter++;
        //         }
        //     }
        // }
    }
}