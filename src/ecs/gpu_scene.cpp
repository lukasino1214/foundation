#include "gpu_scene.hpp"
#include "components.hpp"
#include <graphics/helper.hpp>

namespace foundation {
    GPUScene::GPUScene(Context* _context, Scene* _scene) : context{_context}, scene{_scene} {
        gpu_mesh_group_count = make_task_buffer(context, {
            .size = sizeof(u32),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "mesh group count"
        });
        
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

        gpu_weights = make_task_buffer(context, {
            .size = sizeof(f32),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "weights"
        });

        gpu_animated_mesh_count = make_task_buffer(context, {
            .size = sizeof(u32),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "animated mesh count"
        });

        gpu_animated_meshes = make_task_buffer(context, {
            .size = sizeof(AnimatedMesh),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "animated meshes"
        });

        gpu_animated_mesh_vertices_prefix_sum_work_expansion = make_task_buffer(context, {
            sizeof(PrefixSumWorkExpansion),
            daxa::MemoryFlagBits::NONE,
            "animated mesh vertices prefix sum work expansion"
        });

        gpu_animated_mesh_meshlets_prefix_sum_work_expansion = make_task_buffer(context, {
            sizeof(PrefixSumWorkExpansion),
            daxa::MemoryFlagBits::NONE,
            "animated mesh meshlets prefix sum work expansion"
        });

        gpu_tlas = daxa::TaskTlas{{ .name = "tlas" }};

        sun_light_buffer = make_task_buffer(context, daxa::BufferInfo {
            .size = sizeof(SunLight),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "sun light buffer"
        });

        point_light_buffer = make_task_buffer(context, daxa::BufferInfo {
            .size = sizeof(PointLightsData) + sizeof(PointLight),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "point light buffer"
        });

        spot_light_buffer = make_task_buffer(context, daxa::BufferInfo {
            .size = sizeof(SpotLightsData) + sizeof(SpotLight),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "spot light buffer"
        });

        tile_frustums_buffer = make_task_buffer(context, {
            .size = sizeof(TileFrustum),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "tile frustums buffer"
        });

        tile_data_buffer = make_task_buffer(context, {
            .size = sizeof(TileData),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "tile data buffer"
        });

        tile_indices_buffer = make_task_buffer(context, {
            .size = sizeof(u32),
            .allocate_info = daxa::MemoryFlagBits::NONE,
            .name = "tile indices buffer"
        });
    }

    GPUScene::~GPUScene() {
        context->device.destroy_buffer(gpu_mesh_group_count.get_state().buffers[0]);

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

        context->device.destroy_buffer(gpu_weights.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_animated_mesh_count.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_animated_meshes.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_animated_mesh_vertices_prefix_sum_work_expansion.get_state().buffers[0]);
        context->device.destroy_buffer(gpu_animated_mesh_meshlets_prefix_sum_work_expansion.get_state().buffers[0]);

        context->device.destroy_tlas(gpu_tlas.get_state().tlas[0]);

        context->device.destroy_buffer(sun_light_buffer.get_state().buffers[0]);
        context->device.destroy_buffer(point_light_buffer.get_state().buffers[0]);
        context->device.destroy_buffer(spot_light_buffer.get_state().buffers[0]);

        context->device.destroy_buffer(tile_frustums_buffer.get_state().buffers[0]);
        context->device.destroy_buffer(tile_data_buffer.get_state().buffers[0]);
        context->device.destroy_buffer(tile_indices_buffer.get_state().buffers[0]);

        for(AnimatedMeshInfo& animated_mesh_info : animated_meshes) {
            if(context->device.is_buffer_id_valid(animated_mesh_info.buffer)) {
                context->device.destroy_buffer(animated_mesh_info.buffer);
            }
        }
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

        usize old_mesh_group_count = total_mesh_group_count;
        usize old_animated_mesh_count = animated_meshes.size();
        scene->world->each([&](flecs::entity e, MeshComponent& mesh_component, GPUMeshDirty) {
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

                u32 animated_mesh_index = INVALID_ID;
                if(e.has<AnimationComponent>()) {
                    animated_mesh_index = s_cast<u32>(animated_meshes.size());
                    animated_meshes.push_back({
                        .entity = e,
                        .buffer = {},
                        .weight_offset = s_cast<u32>(total_weight_count),
                    });
                    total_weight_count += e.get<AnimationComponent>()->weights.size();
                }

                u32 gpu_mesh_index = s_cast<u32>(total_mesh_count++);
                dirty_meshes.push_back(gpu_mesh_index);
                mesh.gpu_indices.push_back({
                    .mesh_group_index = gpu_mesh_group_index,
                    .mesh_index = gpu_mesh_index,
                    .animated_mesh_index = animated_mesh_index,
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
            e.remove<GPUMeshDirty>();
        }

    {
        static bool mesh_group_first_time = true;
        if(old_mesh_group_count != total_mesh_group_count || mesh_group_first_time) {
            daxa::BufferId staging_buffer = context->device.create_buffer({
                .size = sizeof(u32),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "mesh group count staging buffer",
            });
            cmd_recorder.destroy_buffer_deferred(staging_buffer);

            std::memcpy(context->device.buffer_host_address(staging_buffer).value(), &total_mesh_group_count, sizeof(u32));
            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = staging_buffer,
                .dst_buffer = gpu_mesh_group_count.get_state().buffers[0],
                .size = sizeof(u32)
            });
            
            mesh_group_first_time = false;
        }

        static bool animated_mesh_first_time = true;
        if(old_animated_mesh_count != animated_meshes.size() || animated_mesh_first_time) {
            daxa::BufferId staging_buffer = context->device.create_buffer({
                .size = sizeof(u32),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "animated mesh count staging buffer",
            });
            cmd_recorder.destroy_buffer_deferred(staging_buffer);

            u32 count = s_cast<u32>(animated_meshes.size());
            std::memcpy(context->device.buffer_host_address(staging_buffer).value(), &count, sizeof(u32));
            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = staging_buffer,
                .dst_buffer = gpu_animated_mesh_count.get_state().buffers[0],
                .size = sizeof(u32)
            });
            
            animated_mesh_first_time = false;
        }
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

        usize total_animated_mesh_count = animated_meshes.size();
        reallocate_buffer(context, cmd_recorder, gpu_animated_meshes, s_cast<u32>(animated_meshes.size() * sizeof(AnimatedMesh)));
        reallocate_buffer(context, cmd_recorder, gpu_weights, s_cast<u32>(total_weight_count * sizeof(f32)));
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
            u32 total_gpu_animated_mesh_count = {};
            for(const UpdateMesh& update_mesh_info : info.update_meshes) {
                MeshGroupData& mesh_group = mesh_group_data[update_mesh_info.mesh_group_index];
                MeshData& mesh_data = mesh_group.meshes[update_mesh_info.mesh_index];

                total_gpu_mesh_count += mesh_data.gpu_indices.size();
                if(mesh_data.mesh_geometry_data.is_animated) {
                    total_gpu_animated_mesh_count += mesh_data.gpu_indices.size();
                }
            }

            daxa::BufferId staging_buffer = context->device.create_buffer({
                .size = total_gpu_mesh_count * sizeof(Mesh),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "mesh manifest upload staging buffer",
            });

            cmd_recorder.destroy_buffer_deferred(staging_buffer);
            Mesh* mesh_staging_ptr = context->device.buffer_host_address_as<Mesh>(staging_buffer).value();
            daxa::DeviceAddress calculated_weights = context->device.buffer_device_address(gpu_weights.get_state().buffers[0]).value();

            daxa::BufferId staging_animated_mesh_buffer = {};
            AnimatedMesh* staging_animated_mesh_ptr = {};
            if(total_gpu_animated_mesh_count != 0) {
                staging_animated_mesh_buffer = context->device.create_buffer({
                    .size = total_gpu_animated_mesh_count * sizeof(AnimatedMesh),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "animated mesh manifest upload staging buffer",
                });
                cmd_recorder.destroy_buffer_deferred(staging_animated_mesh_buffer);
                staging_animated_mesh_ptr = context->device.buffer_host_address_as<AnimatedMesh>(staging_animated_mesh_buffer).value();
            }

            u32 gpu_mesh_index_iter = 0;
            u32 gpu_animated_mesh_index_iter = 0;
            for(const UpdateMesh& update_mesh_info : info.update_meshes) {
                const MeshGroupData& mesh_group = mesh_group_data[update_mesh_info.mesh_group_index];
                const MeshData& mesh_data = mesh_group.meshes[update_mesh_info.mesh_index];
                const MeshGeometryData& mesh_geometry_data = update_mesh_info.mesh_geometry_data;
 
                for(const auto& [gpu_mesh_group_index, gpu_mesh_index, animated_mesh_index] : mesh_data.gpu_indices) {
                    daxa::DeviceAddress aabbs = mesh_geometry_data.meshlet_aabbs;
                    daxa::DeviceAddress bounding_spheres = mesh_geometry_data.bounding_spheres;
                    daxa::DeviceAddress positions = mesh_geometry_data.vertex_positions;
                    daxa::DeviceAddress normals = mesh_geometry_data.vertex_normals;
                    daxa::DeviceAddress uvs = mesh_geometry_data.vertex_uvs;

                    if(mesh_geometry_data.is_animated) {
                        AnimatedMeshInfo& animated_mesh_info = animated_meshes[animated_mesh_index];
                        if(mesh_geometry_data.mesh_buffer.is_empty() && context->device.is_buffer_id_valid(animated_mesh_info.buffer)) {
                            cmd_recorder.destroy_buffer_deferred(animated_mesh_info.buffer);
                        } else {
                            usize total_size = 0;
                            total_size += mesh_geometry_data.meshlet_count * sizeof(AABB);
                            total_size += mesh_geometry_data.meshlet_count * sizeof(MeshletBoundingSpheres);
                            total_size += mesh_geometry_data.vertex_count * sizeof(f32vec3);
                            total_size += mesh_geometry_data.vertex_count * sizeof(u32);
                            total_size += mesh_geometry_data.vertex_count * sizeof(u32);
    
                            animated_mesh_info.buffer = context->device.create_buffer(daxa::BufferInfo {
                                .size = total_size,
                                .name = fmt::format("{} animated buffer", context->device.buffer_info(mesh_geometry_data.mesh_buffer).value().name.c_str().data()).substr(0, 59)
                            });

                            aabbs = context->device.buffer_device_address(animated_mesh_info.buffer).value();
                            bounding_spheres = aabbs + mesh_geometry_data.meshlet_count * sizeof(AABB);
                            positions = bounding_spheres + mesh_geometry_data.meshlet_count * sizeof(MeshletBoundingSpheres);
                            normals = positions + mesh_geometry_data.vertex_count * sizeof(f32vec3);
                            uvs = normals + mesh_geometry_data.vertex_count * sizeof(u32);
   
                            AnimatedMesh animated_mesh = {
                                .meshlet_count = mesh_geometry_data.meshlet_count,
                                .vertex_count = mesh_geometry_data.vertex_count,
                                .aabb = mesh_geometry_data.aabb,
                                .meshlets = mesh_geometry_data.meshlets,
                                .bounding_spheres = bounding_spheres,
                                .simplification_errors = mesh_geometry_data.simplification_errors,
                                .meshlet_aabbs = aabbs,
                                .micro_indices = mesh_geometry_data.micro_indices,
                                .indirect_vertices = mesh_geometry_data.indirect_vertices,
                                .indices = mesh_geometry_data.indices,
                                .original_vertex_positions = mesh_geometry_data.vertex_positions,
                                .original_vertex_normals = mesh_geometry_data.vertex_normals,
                                .original_vertex_uvs = mesh_geometry_data.vertex_uvs,
                                .morph_target_position_count = mesh_geometry_data.morph_target_position_count,
                                .morph_target_normal_count = mesh_geometry_data.morph_target_normal_count,
                                .morph_target_uv_count = mesh_geometry_data.morph_target_uv_count,
                                .morph_target_positions = mesh_geometry_data.morph_target_positions,
                                .morph_target_normals = mesh_geometry_data.morph_target_normals,
                                .morph_target_uvs = mesh_geometry_data.morph_target_uvs,
                                .calculated_weights = calculated_weights + animated_mesh_info.weight_offset * sizeof(f32),
                                .vertex_positions = positions,
                                .vertex_normals = normals,
                                .vertex_uvs = uvs,
                            };

                            std::memcpy(staging_animated_mesh_ptr + gpu_animated_mesh_index_iter, &animated_mesh, sizeof(AnimatedMesh));
                            cmd_recorder.copy_buffer_to_buffer({
                                .src_buffer = staging_animated_mesh_buffer,
                                .dst_buffer = gpu_animated_meshes.get_state().buffers[0],
                                .src_offset = gpu_animated_mesh_index_iter * sizeof(AnimatedMesh),
                                .dst_offset = animated_mesh_index * sizeof(AnimatedMesh),
                                .size = sizeof(AnimatedMesh),
                            });
                            gpu_animated_mesh_index_iter++;
                        }
                    }

                    Mesh mesh = {
                        .mesh_group_index = gpu_mesh_group_index,
                        .manifest_index = mesh_geometry_data.manifest_index,
                        .material_index = mesh_geometry_data.material_index,
                        .meshlet_count = mesh_geometry_data.meshlet_count,
                        .vertex_count = mesh_geometry_data.vertex_count,
                        .aabb = mesh_geometry_data.aabb,
                        .meshlets = mesh_geometry_data.meshlets,
                        .bounding_spheres = bounding_spheres,
                        .simplification_errors = mesh_geometry_data.simplification_errors,
                        .meshlet_aabbs = aabbs,
                        .micro_indices = mesh_geometry_data.micro_indices,
                        .indirect_vertices = mesh_geometry_data.indirect_vertices,
                        .vertex_positions = positions,
                        .vertex_normals = normals,
                        .vertex_uvs = uvs,
                    };
                    std::memcpy(mesh_staging_ptr + gpu_mesh_index_iter, &mesh, sizeof(Mesh));

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

        if(!animated_meshes.empty()) {
            daxa::BufferId staging_buffer = context->device.create_buffer({
                .size = total_weight_count * sizeof(f32),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "weights staging buffer",
            });
            cmd_recorder.destroy_buffer_deferred(staging_buffer);
            f32* staging_ptr = context->device.buffer_host_address_as<f32>(staging_buffer).value();

            for(const AnimatedMeshInfo& animated_mesh_info : animated_meshes) {
                const AnimationComponent* animation_component = animated_mesh_info.entity.get<AnimationComponent>();

                std::memcpy(staging_ptr + animated_mesh_info.weight_offset, animation_component->weights.data(), animation_component->weights.size() * sizeof(f32));
                cmd_recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_weights.get_state().buffers[0],
                    .src_offset = animated_mesh_info.weight_offset * sizeof(f32),
                    .dst_offset = animated_mesh_info.weight_offset * sizeof(f32),
                    .size = animation_component->weights.size() * sizeof(f32),
                });
            }
        }

        std::vector<flecs::entity> moved_entities = {};
        scene->world->each([&](flecs::entity e, MeshComponent&, GPUTransformDirty) {
            moved_entities.push_back(e);
        });

        if(!moved_entities.empty()) {
            daxa::BufferId staging_buffer = context->device.create_buffer({
                .size = moved_entities.size() * sizeof(f32mat4x3),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "transform upload staging buffer",
            });

            cmd_recorder.destroy_buffer_deferred(staging_buffer);
            f32mat4x3* staging_ptr = context->device.buffer_host_address_as<f32mat4x3>(staging_buffer).value();

            for(u32 i = 0; i < moved_entities.size(); i++) {
                flecs::entity e = moved_entities[i];

                staging_ptr[i] = e.get<GlobalMatrix>()->matrix;
                cmd_recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_transforms.get_state().buffers[0],
                    .src_offset = i * sizeof(f32mat4x3),
                    .dst_offset = e.get<MeshComponent>()->mesh_group_index.value() * sizeof(f32mat4x3),
                    .size = sizeof(f32mat4x3),
                });

                e.remove<GPUTransformDirty>();
            }
        } else {
            for(flecs::entity e : moved_entities) {
                e.remove<GPUTransformDirty>();
            }
        }

        {
            static bool first_time = true;
            if(first_time) {
                daxa::BufferId staging_buffer = context->device.create_buffer(daxa::BufferInfo {
                    .size = sizeof(SunLight),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "sun staging_buffer"
                });
                cmd_recorder.destroy_buffer_deferred(staging_buffer);

                SunLight light = {
                    .direction = { -1.0f, -1.0f, -1.0f },
                    .color = { 1.0f, 1.0f, 1.0f },
                    .intensity = 1.0f
                };

                std::memcpy(context->device.buffer_host_address(staging_buffer).value(), &light, sizeof(SunLight));

                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = sun_light_buffer.get_state().buffers[0],
                    .size = sizeof(SunLight)
                });

                first_time = false;
            }
        }

        {
            std::vector<flecs::entity> entities = {};
            scene->world->each([&](flecs::entity e, PointLightComponent& point_light_component, GPULightDirty) {
                if(!point_light_component.gpu_index.has_value()) {
                    point_light_component.gpu_index = total_point_light_count++;
                }

                entities.push_back(e);
            });

            reallocate_buffer<PointLightsData>(context, cmd_recorder, point_light_buffer, s_cast<u32>(total_point_light_count * sizeof(PointLight) + sizeof(PointLightsData)), [&](const daxa::BufferId& buffer) -> PointLightsData {
                return PointLightsData {
                    .count = static_cast<u32>(total_point_light_count),
                    .bitmask_size = round_up_div(s_cast<u32>(total_point_light_count), 32),
                    .point_lights = context->device.buffer_device_address(buffer).value() + sizeof(PointLightsData)
                };
            });

            if(!entities.empty()) {
                daxa::BufferId staging_buffer = context->device.create_buffer({
                    .size = entities.size() * sizeof(PointLight),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "point light staging buffer"
                });
                cmd_recorder.destroy_buffer_deferred(staging_buffer);

                for(u32 i = 0; i < entities.size(); i++) {
                    const PointLightComponent* p = entities[i].get<PointLightComponent>();
                    PointLight light = {
                        .position = p->position,
                        .color = p->color,
                        .intensity = p->intensity,
                        .range = p->range,
                    };

                    std::memcpy(context->device.buffer_host_address(staging_buffer).value() + sizeof(PointLight) * i, &light, sizeof(PointLight));
                    cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                        .src_buffer = staging_buffer,
                        .dst_buffer = point_light_buffer.get_state().buffers[0],
                        .src_offset = sizeof(PointLight) * i,
                        .dst_offset = sizeof(PointLightsData) + sizeof(PointLight) * p->gpu_index.value(),
                        .size = sizeof(PointLight)
                    });
                }
            }

            for(flecs::entity e : entities) {
                e.remove<GPULightDirty>();
            }
        }

        {
            std::vector<flecs::entity> entities = {};
            scene->world->each([&](flecs::entity e, SpotLightComponent& spot_light_component, GPULightDirty) {
                if(!spot_light_component.gpu_index.has_value()) {
                    spot_light_component.gpu_index = total_spot_light_count++;
                }

                entities.push_back(e);
            });

            reallocate_buffer<SpotLightsData>(context, cmd_recorder, spot_light_buffer, s_cast<u32>(total_spot_light_count * sizeof(SpotLight) + sizeof(SpotLightsData)), [&](const daxa::BufferId& buffer) -> SpotLightsData {
                return SpotLightsData {
                    .count = static_cast<u32>(total_spot_light_count),
                    .bitmask_size = round_up_div(s_cast<u32>(total_spot_light_count), 32),
                    .spot_lights = context->device.buffer_device_address(buffer).value() + sizeof(SpotLightsData)
                };
            });

            if(!entities.empty()) {
                daxa::BufferId staging_buffer = context->device.create_buffer({
                    .size = entities.size() * sizeof(SpotLight),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "spot light staging buffer"
                });
                cmd_recorder.destroy_buffer_deferred(staging_buffer);

                for(u32 i = 0; i < entities.size(); i++) {
                    const SpotLightComponent* s = entities[i].get<SpotLightComponent>();
                    SpotLight light = {
                        .position = s->position,
                        .direction = s->direction,
                        .color = s->color,
                        .intensity = s->intensity,
                        .range = s->range,
                        .inner_cone_angle = s->inner_cone_angle,
                        .outer_cone_angle = s->outer_cone_angle,
                    };

                    std::memcpy(context->device.buffer_host_address(staging_buffer).value() + sizeof(SpotLight) * i, &light, sizeof(SpotLight));
                    cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                        .src_buffer = staging_buffer,
                        .dst_buffer = spot_light_buffer.get_state().buffers[0],
                        .src_offset = sizeof(SpotLight) * i,
                        .dst_offset = sizeof(SpotLightsData) + sizeof(SpotLight) * s->gpu_index.value(),
                        .size = sizeof(SpotLight)
                    });
                }
            }

            for(flecs::entity e : entities) {
                e.remove<GPULightDirty>();
            }
        }

        if(screen_resized) {
            const u32vec2 size = context->shader_globals.render_target_size;

            context->device.destroy_buffer(tile_frustums_buffer.get_state().buffers[0]);
            tile_frustums_buffer.set_buffers({ .buffers = std::array{
                context->device.create_buffer(daxa::BufferInfo {
                    .size = sizeof(TileFrustum) * std::max(round_up_div(size.x, PIXELS_PER_FRUSTUM), 1u) * std::max(round_up_div(size.y, PIXELS_PER_FRUSTUM), 1u),
                    .allocate_info = daxa::MemoryFlagBits::NONE,
                    .name = "tile frustums buffer"
                })
            }});

            context->device.destroy_buffer(tile_data_buffer.get_state().buffers[0]);
            tile_data_buffer.set_buffers({ .buffers = std::array{
                context->device.create_buffer(daxa::BufferInfo {
                    .size = sizeof(TileData) * std::max(round_up_div(size.x, PIXELS_PER_FRUSTUM), 1u) * std::max(round_up_div(size.y, PIXELS_PER_FRUSTUM), 1u),
                    .allocate_info = daxa::MemoryFlagBits::NONE,
                    .name = "tile data buffer"
                })
            }});

            context->device.destroy_buffer(tile_indices_buffer.get_state().buffers[0]);
            const u32 total_amount_of_lights = s_cast<u32>(total_point_light_count + total_spot_light_count);
            tile_indices_buffer.set_buffers({ .buffers = std::array{
                context->device.create_buffer(daxa::BufferInfo {
                    .size = 2 * sizeof(u32) * std::max(round_up_div(total_amount_of_lights, 32), 1u) * std::max(round_up_div(size.x, PIXELS_PER_FRUSTUM), 1u) * std::max(round_up_div(size.y, PIXELS_PER_FRUSTUM), 1u),
                    .allocate_info = daxa::MemoryFlagBits::NONE,
                    .name = "tile indices buffer"
                })
            }});

            screen_resized = false;
        }

        cmd_recorder.pipeline_barrier(daxa::MemoryBarrierInfo {
            .src_access = daxa::AccessConsts::TRANSFER_WRITE,
            .dst_access = daxa::AccessConsts::READ,
        });

        if((!info.update_meshes.empty() || !info.update_mesh_groups.empty() || !moved_entities.empty()) && false) {
            PROFILE_SCOPE_NAMED(build_tlas);

            std::vector<daxa_BlasInstanceData> blas_instances = {};
            for(const auto& [_, mesh_group] : mesh_group_data) {
                for(flecs::entity entity : mesh_group.entites) {
                    for(const auto& mesh : mesh_group.meshes) {
                        if(mesh.mesh_geometry_data.mesh_buffer.is_empty()) { continue; }
                        glm::mat3x4 new_matrix = glm::transpose(entity.get<GlobalMatrix>()->matrix);
                        blas_instances.push_back(daxa_BlasInstanceData {
                            .transform = *r_cast<daxa_f32mat3x4*>(&new_matrix),
                            .instance_custom_index = 0,
                            .mask = 0xFF,
                            .instance_shader_binding_table_record_offset = 0,
                            .flags = DAXA_GEOMETRY_INSTANCE_FORCE_OPAQUE,
                            .blas_device_address = context->device.blas_device_address(std::bit_cast<daxa::BlasId>(mesh.mesh_geometry_data.blas)).value(),
                        });
                    }
                }
            }

            daxa::BufferId staging_blas_instances_buffer = context->device.create_buffer({
                .size = sizeof(daxa_BlasInstanceData) * blas_instances.size(),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "staging_blas_instances_buffer",
            });
            std::memcpy(context->device.buffer_host_address(staging_blas_instances_buffer).value(), blas_instances.data(), sizeof(daxa_BlasInstanceData) * blas_instances.size());

            daxa::BufferId blas_instances_buffer = context->device.create_buffer({
                .size = sizeof(daxa_BlasInstanceData) * blas_instances.size(),
                .name = "blas_instances_buffer",
            });
            cmd_recorder.destroy_buffer_deferred(blas_instances_buffer);

            cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                .src_buffer = staging_blas_instances_buffer,
                .dst_buffer = blas_instances_buffer,
                .size = sizeof(daxa_BlasInstanceData) * blas_instances.size()
            });
            cmd_recorder.destroy_buffer_deferred(staging_blas_instances_buffer);

            const daxa_u32 ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT = 256;
            auto round_up_to_multiple = [](daxa_u32 value, daxa_u32 multiple_of) -> daxa_u32 {
                return ((value + multiple_of - 1) / multiple_of) * multiple_of;
            };

            auto tlas_blas_instances_infos = std::array{daxa::TlasInstanceInfo{
                .data = context->device.buffer_device_address(blas_instances_buffer).value(),
                .count = s_cast<u32>(blas_instances.size()),
                .is_data_array_of_pointers = false,
                .flags = daxa::GeometryFlagBits::NONE,
            }};

            auto tlas_build_info = daxa::TlasBuildInfo {
                .flags = daxa::AccelerationStructureBuildFlagBits::PREFER_FAST_TRACE,
                .instances = tlas_blas_instances_infos,
            };

            const daxa::AccelerationStructureBuildSizesInfo tlas_build_sizes = context->device.tlas_build_sizes(tlas_build_info);
            if(!gpu_tlas.get_state().tlas.empty()) { context->device.destroy_tlas(gpu_tlas.get_state().tlas[0]); }
            daxa::TlasId tlas = context->device.create_tlas({
                .size = round_up_to_multiple(s_cast<u32>(tlas_build_sizes.acceleration_structure_size), ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT),
                .name = "tlas"
            });
            gpu_tlas.set_tlas({ .tlas = std::span{&tlas, 1}});

            daxa::BufferId tlas_scratch_buffer = context->device.create_buffer({
                .size = round_up_to_multiple(s_cast<u32>(tlas_build_sizes.build_scratch_size), ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT),
            });
            cmd_recorder.destroy_buffer_deferred(tlas_scratch_buffer);

            tlas_build_info.dst_tlas = tlas;
            tlas_build_info.scratch_data = context->device.buffer_device_address(tlas_scratch_buffer).value();
        
            cmd_recorder.build_acceleration_structures({
                .tlas_build_infos = std::array{tlas_build_info},
            });
        }

        return cmd_recorder.complete_current_commands();
    }
}