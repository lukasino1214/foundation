#include "gpu_scene.hpp"
#include "components.hpp"
#include <graphics/helper.hpp>

namespace foundation {
    GPUScene::GPUScene(Context* _context, Scene* _scene) : context{_context}, scene{_scene} {
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

    auto GPUScene::update(const UpdateInfo& info) -> daxa::ExecutableCommandList {
        PROFILE_SCOPE;
        daxa::CommandRecorder cmd_recorder = context->device.create_command_recorder({ .name = "update gpu scene"});

        for(const UpdateMeshGroup& update_mesh_group : info.update_mesh_groups) {
            MeshGroupData mesh_group = {};
            mesh_group.meshes.resize(update_mesh_group.mesh_count);
            mesh_group_data[update_mesh_group.mesh_group_index] = mesh_group;
        }

        for(const UpdateMesh& update_mesh : info.update_meshes) {
            auto& mesh = mesh_group_data[update_mesh.mesh_group_index].meshes[update_mesh.mesh_index];
            mesh.mesh_geometry_data = update_mesh.mesh_geometry_data;
        }

        std::vector<flecs::entity> deletion_queue = {};
        std::vector<u32> dirty_meshes = {};
        std::vector<MeshGroupToMeshesMapping> dirty_mesh_groups = {};

        scene->world->each([&](flecs::entity e, MeshComponent& mesh_component, GPUDirty) {
            MeshGroupData& mesh_group = mesh_group_data[mesh_component.mesh_group_manifest_entry_index.value()];
            mesh_group.entites.push_back(e);
            
            u32 gpu_mesh_group_index = s_cast<u32>(total_mesh_group_count++);
            mesh_component.mesh_group_index = gpu_mesh_group_index;
            
            dirty_mesh_groups.push_back(MeshGroupToMeshesMapping {
                .mesh_group_manifest_index = mesh_component.mesh_group_manifest_entry_index.value(),
                .mesh_group_index = gpu_mesh_group_index,
                .mesh_offset = s_cast<u32>(total_mesh_count),
                .mesh_count = s_cast<u32>(mesh_group.meshes.size()),
            });

            for(u32 mesh_index = 0; mesh_index < mesh_group.meshes.size(); mesh_index++) {
                MeshData& mesh = mesh_group.meshes[mesh_index];

                u32 gpu_mesh_index = s_cast<u32>(total_mesh_count++);
                dirty_meshes.push_back(gpu_mesh_index);
                mesh.gpu_indices.push_back({
                    .mesh_group_index = gpu_mesh_group_index,
                    .mesh_index = gpu_mesh_index,
                });

                BinaryAlphaMode alpha_mode = s_cast<BinaryAlphaMode>(mesh.mesh_geometry_data.material_type);
                u32 meshlet_count = mesh.mesh_geometry_data.meshlet_count;
                total_meshlet_count += meshlet_count;
        
                if(mesh.mesh_geometry_data.manifest_index != INVALID_ID) { 
                    switch (alpha_mode) {
                        case BinaryAlphaMode::Opaque: {
                            total_opaque_mesh_count++; 
                            total_opaque_meshlet_count += meshlet_count;
                            break; 
                        }
                        case BinaryAlphaMode::Masked: { 
                            total_masked_mesh_count++; 
                            total_masked_meshlet_count += meshlet_count;
                            break; }
                        case BinaryAlphaMode::Transparent: { 
                            total_transparent_mesh_count++; 
                            total_transparent_meshlet_count += meshlet_count;
                            break; 
                        }
                        default: { 
                            fmt::println("something went wrong. so boom! {}", s_cast<u32>(alpha_mode)); 
                            std::exit(-1); 
                            break; 
                        }
                    }
                } else {
                    total_opaque_meshlet_count += meshlet_count;
                    total_opaque_mesh_count++;
                }
            }

            deletion_queue.push_back(e);
        });

        for(flecs::entity e : deletion_queue) {
            e.remove<GPUDirty>();
        }

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

        if(!info.update_meshes.empty()) {
            u32 total_gpu_mesh_count = {};
            for(const UpdateMesh& update_mesh_info : info.update_meshes) {
                MeshGroupData& mesh_group = mesh_group_data[update_mesh_info.mesh_group_index];
                MeshData& mesh_data = mesh_group.meshes[update_mesh_info.mesh_index];

                total_gpu_mesh_count += mesh_data.gpu_indices.size();
            }

            daxa::BufferId staging_buffer = context->device.create_buffer({
                .size = total_gpu_mesh_count * sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "mesh manifest upload staging buffer",
            });

            cmd_recorder.destroy_buffer_deferred(staging_buffer);
            Mesh* staging_ptr = context->device.buffer_host_address_as<Mesh>(staging_buffer).value();

            u32 gpu_mesh_index_iter = 0;
            for(const UpdateMesh& update_mesh_info : info.update_meshes) {
                const MeshGroupData& mesh_group = mesh_group_data[update_mesh_info.mesh_group_index];
                const MeshData& mesh_data = mesh_group.meshes[update_mesh_info.mesh_index];
                const MeshGeometryData& mesh_geometry_data = update_mesh_info.mesh_geometry_data;
 
                for(auto [gpu_mesh_group_index, gpu_mesh_index] : mesh_data.gpu_indices) {
                    Mesh mesh = {
                        .mesh_group_index = gpu_mesh_group_index,
                        .manifest_index = mesh_geometry_data.manifest_index,
                        .material_index = mesh_geometry_data.material_index,
                        .meshlet_count = mesh_geometry_data.meshlet_count,
                        .vertex_count = mesh_geometry_data.vertex_count,
                        .aabb = mesh_geometry_data.aabb,
                        .meshlets = mesh_geometry_data.meshlets,
                        .bounding_spheres = mesh_geometry_data.bounding_spheres,
                        .simplification_errors = mesh_geometry_data.simplification_errors,
                        .meshlet_aabbs = mesh_geometry_data.meshlet_aabbs,
                        .micro_indices = mesh_geometry_data.micro_indices,
                        .indirect_vertices = mesh_geometry_data.indirect_vertices,
                        .primitive_indices = mesh_geometry_data.primitive_indices,
                        .vertex_positions = mesh_geometry_data.vertex_positions,
                        .vertex_normals = mesh_geometry_data.vertex_normals,
                        .vertex_uvs = mesh_geometry_data.vertex_uvs,
                    };
                    std::memcpy(staging_ptr + gpu_mesh_index_iter, &mesh, sizeof(Mesh));

                    cmd_recorder.copy_buffer_to_buffer({
                        .src_buffer = staging_buffer,
                        .dst_buffer = gpu_meshes.get_state().buffers[0],
                        .src_offset = gpu_mesh_index_iter * sizeof(Mesh),
                        .dst_offset = gpu_mesh_index * sizeof(Mesh),
                        .size = sizeof(Mesh),
                    });

                    gpu_mesh_index_iter++;
                }
            }
        }

        return cmd_recorder.complete_current_commands();
    }
}