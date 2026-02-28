#include "gpu_scene.hpp"

#include "gpu_context.hpp"
#include "helper.hpp"
#include <ecs/scene.hpp>
#include <asset/asset_manager.hpp>

namespace foundation {
    GPUScene::GPUScene(GPUContext* _gpu_context, Scene* _scene, AssetManager* _asset_manager) : gpu_context{_gpu_context}, scene{_scene}, asset_manager{_asset_manager} {
        gpu_mesh_group_instance_count = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(u32),
            .name = "mesh group instance count"
        });
        
        gpu_mesh_group_instances = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(MeshGroupInstance),
            .name = "mesh group instances"
        });

        gpu_transforms = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(f32mat4x3),
            .name = "transforms"
        });

        gpu_mesh_instances = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(MeshInstance),
            .name = "mesh instances"
        });

        gpu_mesh_instance_indices = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(u32),
            .name = "mesh instance indices"
        });

        gpu_culled_mesh_instances_data = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(CulledMeshInstancesData),
            .name = "culled mesh instances data"
        });

        gpu_opaque_prefix_sum_work_expansion_mesh = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(PrefixSumWorkExpansion),
            .name = "opaque prefix sum work expansion mesh"
        });

        gpu_masked_prefix_sum_work_expansion_mesh = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(PrefixSumWorkExpansion),
            .name = "masked prefix sum work expansion mesh"
        });

        gpu_blended_prefix_sum_work_expansion_mesh = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(PrefixSumWorkExpansion),
            .name = "blended prefix sum work expansion mesh"
        });

        gpu_culled_meshlet_instances = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(MeshletInstance),
            .name = "culled meshlets instances"
        });

        gpu_opaque_culled_meshlet_instances_region = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(CulledMeshletInstancesRegion),
            .name = "opaque culled meshlets instance region"
        });

        gpu_masked_culled_meshlet_instances_region = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(CulledMeshletInstancesRegion),
            .name = "masked culled meshlets instance region"
        });

        gpu_blended_culled_meshlet_instances_region = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(CulledMeshletInstancesRegion),
            .name = "blended culled meshlets instance region"
        });
        
        gpu_tlas = daxa::TaskTlas{{ .name = "tlas" }};

        gpu_point_lights = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(PointLightsData),
            .name = "point lights"
        });

        gpu_spot_lights = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(SpotLightsData),
            .name = "spot lights"
        });
    }
    GPUScene::~GPUScene() {
        gpu_context->device.destroy_buffer(gpu_mesh_group_instance_count.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_mesh_group_instances.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_transforms.get_state().buffers[0]);

        gpu_context->device.destroy_buffer(gpu_mesh_instances.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_mesh_instance_indices.get_state().buffers[0]);

        gpu_context->device.destroy_buffer(gpu_culled_mesh_instances_data.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_opaque_prefix_sum_work_expansion_mesh.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_masked_prefix_sum_work_expansion_mesh.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_blended_prefix_sum_work_expansion_mesh.get_state().buffers[0]);

        gpu_context->device.destroy_buffer(gpu_opaque_culled_meshlet_instances_region.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_masked_culled_meshlet_instances_region.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_blended_culled_meshlet_instances_region.get_state().buffers[0]);

        gpu_context->device.destroy_buffer(gpu_culled_meshlet_instances.get_state().buffers[0]);

        gpu_context->device.destroy_tlas(gpu_tlas.get_state().tlas[0]);
        gpu_context->device.destroy_buffer(gpu_point_lights.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_spot_lights.get_state().buffers[0]);
    }

    void GPUScene::update(daxa::CommandRecorder& cmd_recorder, const ResourceUploadInfos& upload_infos) {
        PROFILE_SCOPE;

        auto write_mesh_instance = [&](u32 mesh_instance_index, u32 mesh_manifest_index, u32 mesh_group_instance_index) {
            MeshInstance out_mesh_instance = {};
            out_mesh_instance.mesh_group_index = mesh_group_instance_index;
            out_mesh_instance.material_index = INVALID_ID;

            if(mesh_manifest_index < asset_manager->mesh_manifests.size()) {
                const MeshManifest& mesh_manifest = asset_manager->mesh_manifests[mesh_manifest_index];
                if(mesh_manifest.render_info.has_value()) {
                    const auto& render_info = mesh_manifest.render_info.value();
                    out_mesh_instance.mesh_buffer = render_info.mesh_buffer;
                    out_mesh_instance.meshlets = render_info.mesh.meshlets;
                    out_mesh_instance.indirect_vertices = render_info.mesh.indirect_vertices;
                    out_mesh_instance.micro_indices = render_info.mesh.micro_indices;
                    out_mesh_instance.bounding_spheres = render_info.mesh.bounding_spheres;
                    out_mesh_instance.aabbs = render_info.mesh.aabbs;
                    out_mesh_instance.positions = render_info.mesh.positions;
                    out_mesh_instance.normals = render_info.mesh.normals;
                    out_mesh_instance.uvs = render_info.mesh.uvs;
                    out_mesh_instance.mesh_aabb = render_info.mesh.mesh_aabb;
                    out_mesh_instance.material_index = render_info.mesh.material_index;
                    out_mesh_instance.meshlet_count = render_info.meshlet_count;
                }
            }

            MeshInstance &old_mesh_instance = mesh_instances[mesh_instance_index];
            if(std::memcmp(&old_mesh_instance, &out_mesh_instance, sizeof(MeshInstance)) == 0) { return; }

            old_mesh_instance = out_mesh_instance;
            dirty_mesh_instance_indices.push_back(mesh_instance_index);
        };

        usize old_mesh_group_count = total_mesh_group_count;
        std::vector<flecs::entity> deletion_queue = {};
        scene->world->each([&](flecs::entity e, MeshComponent& mesh_component, MeshComponentDirty) {
            const MeshGroupManifest& mesh_group_manifest = asset_manager->mesh_group_manifests[mesh_component.mesh_group_manifest_entry_index];
            const AssetManifest& asset_manifest = asset_manager->asset_manifests[mesh_group_manifest.asset_manifest_index];
            
            if(!mesh_component.mesh_group_index.has_value()) {
                u32 mesh_group_instance_index = s_cast<u32>(total_mesh_group_count++);
                mesh_component.mesh_group_index = mesh_group_instance_index;

                CPUMeshGroupInstance cpu_mesh_group_instance = {
                    .mesh_group_instance_index = mesh_group_instance_index,
                    .mesh_instance_indices = {},
                };
                cpu_mesh_group_instance.mesh_instance_indices.reserve(mesh_group_manifest.mesh_count);

                mesh_group_instances.push_back({
                    .mesh_indices_offset = s_cast<u32>(mesh_instances.size()),
                    .count = mesh_group_manifest.mesh_count,
                });

                for(u32 mesh_index = 0; mesh_index < mesh_group_manifest.mesh_count; mesh_index++) {
                    const u32 mesh_manifest_index = asset_manifest.mesh_manifest_offset + mesh_group_manifest.asset_local_mesh_manifest_indices_offset + mesh_index;
                    const u32 mesh_instance_index = s_cast<u32>(mesh_instances.size());
                    cpu_mesh_group_instance.mesh_instance_indices.push_back(mesh_instance_index);

                    mesh_instances.push_back({});
                    mesh_manifest_to_mesh_instances[mesh_manifest_index].push_back(mesh_instance_index);
                    write_mesh_instance(mesh_instance_index, mesh_manifest_index, mesh_group_instance_index);
                }

                mesh_group_data[mesh_component.mesh_group_manifest_entry_index].mesh_group_instances[e.id()] = std::move(cpu_mesh_group_instance);
            }
            deletion_queue.push_back(e);
        });

        for(flecs::entity e : deletion_queue) {
            e.remove<MeshComponentDirty>();
        }

        if(!upload_infos.meshes.empty()) {
            for(const auto& mesh_upload_info : upload_infos.meshes) {
                if(mesh_upload_info.asset_manifest_index >= asset_manager->asset_manifests.size()) { continue; }
                const AssetManifest& asset_manifest = asset_manager->asset_manifests[mesh_upload_info.asset_manifest_index];
                const u32 mesh_manifest_index = asset_manifest.mesh_manifest_offset + mesh_upload_info.mesh_manifest_index;

                if(auto it = mesh_manifest_to_mesh_instances.find(mesh_manifest_index); it != mesh_manifest_to_mesh_instances.end()) {
                    for(const u32 mesh_instance_index : it->second) {
                        write_mesh_instance(mesh_instance_index, mesh_manifest_index, mesh_instances[mesh_instance_index].mesh_group_index);
                    }
                }
            }
        }

        total_mesh_count = mesh_instances.size();
        if(!dirty_mesh_instance_indices.empty()) {
            total_mesh_count = 0;
            total_opaque_mesh_count = 0;
            total_masked_mesh_count = 0;
            total_blended_mesh_count = 0;

            total_meshlet_count = 0;
            total_opaque_meshlet_count = 0;
            total_masked_meshlet_count = 0;
            total_blended_meshlet_count = 0;

            for(const auto& mesh_instance : mesh_instances) {
                total_mesh_count += 1;

                total_meshlet_count += mesh_instance.meshlet_count;
                if(mesh_instance.material_index == INVALID_ID) {
                    total_opaque_mesh_count += 1;
                    total_opaque_meshlet_count += mesh_instance.meshlet_count;
                    continue;
                }

                switch(asset_manager->material_manifests[mesh_instance.material_index].material_type) {
                    case MaterialType::Opaque: {
                        total_opaque_mesh_count += 1;
                        total_opaque_meshlet_count += mesh_instance.meshlet_count; 
                        break;
                    }
                    case MaterialType::Masked: {
                        total_masked_mesh_count += 1;
                        total_masked_meshlet_count += mesh_instance.meshlet_count; 
                        break;
                    }
                    case MaterialType::Blend: {
                        total_blended_mesh_count += 1;
                        total_blended_meshlet_count += mesh_instance.meshlet_count; 
                        break;
                    }
                }
            }
        }

        {
            static bool first_time = true;
            const bool mesh_data_changed = old_mesh_group_count != total_mesh_group_count || first_time;
            if(mesh_data_changed) {
                std::vector<MeshGroupInstance> cpu_mesh_group_instances = {};
                cpu_mesh_group_instances.resize(total_mesh_group_count);

                std::vector<u32> mesh_indices = {};
                mesh_indices.reserve(total_mesh_count);

                for(const auto& [_, cpu_mesh_group_data] : mesh_group_data) {
                    for(const auto& [__, cpu_mesh_group_instance] : cpu_mesh_group_data.mesh_group_instances) {
                        MeshGroupInstance& gpu_mesh_group_instance = cpu_mesh_group_instances[cpu_mesh_group_instance.mesh_group_instance_index];
                        gpu_mesh_group_instance.mesh_indices_offset = s_cast<u32>(mesh_indices.size());
                        gpu_mesh_group_instance.count = s_cast<u32>(cpu_mesh_group_instance.mesh_instance_indices.size());
                        mesh_indices.insert(mesh_indices.end(), cpu_mesh_group_instance.mesh_instance_indices.begin(), cpu_mesh_group_instance.mesh_instance_indices.end());
                    }
                }

                if(!cpu_mesh_group_instances.empty()) {
                    reallocate_buffer(gpu_context, cmd_recorder, gpu_mesh_group_instances, s_cast<u32>(cpu_mesh_group_instances.size() * sizeof(MeshGroupInstance)));

                    daxa::BufferId staging_buffer = gpu_context->device.create_buffer({
                        .size = cpu_mesh_group_instances.size() * sizeof(MeshGroupInstance),
                        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                        .name = "mesh groups staging buffer",
                    });
                    cmd_recorder.destroy_buffer_deferred(staging_buffer);
                    std::memcpy(gpu_context->device.buffer_host_address(staging_buffer).value(), cpu_mesh_group_instances.data(), cpu_mesh_group_instances.size() * sizeof(MeshGroupInstance));
                    cmd_recorder.copy_buffer_to_buffer({
                        .src_buffer = staging_buffer,
                        .dst_buffer = gpu_mesh_group_instances.get_state().buffers[0],
                        .size = cpu_mesh_group_instances.size() * sizeof(MeshGroupInstance),
                    });
                }

                if(!mesh_instances.empty()) {
                    reallocate_buffer(gpu_context, cmd_recorder, gpu_mesh_instances, s_cast<u32>(mesh_instances.size() * sizeof(MeshInstance)));

                    daxa::BufferId staging_buffer = gpu_context->device.create_buffer({
                        .size = mesh_instances.size() * sizeof(MeshInstance),
                        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                        .name = "meshes staging buffer",
                    });
                    cmd_recorder.destroy_buffer_deferred(staging_buffer);
                    std::memcpy(gpu_context->device.buffer_host_address(staging_buffer).value(), mesh_instances.data(), mesh_instances.size() * sizeof(MeshInstance));
                    cmd_recorder.copy_buffer_to_buffer({
                        .src_buffer = staging_buffer,
                        .dst_buffer = gpu_mesh_instances.get_state().buffers[0],
                        .size = mesh_instances.size() * sizeof(MeshInstance),
                    });
                }

                if(!mesh_indices.empty()) {
                    reallocate_buffer(gpu_context, cmd_recorder, gpu_mesh_instance_indices, s_cast<u32>(mesh_indices.size() * sizeof(u32)));

                    daxa::BufferId staging_buffer = gpu_context->device.create_buffer({
                        .size = mesh_indices.size() * sizeof(u32),
                        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                        .name = "mesh indices staging buffer",
                    });
                    cmd_recorder.destroy_buffer_deferred(staging_buffer);
                    std::memcpy(gpu_context->device.buffer_host_address(staging_buffer).value(), mesh_indices.data(), mesh_indices.size() * sizeof(u32));
                    cmd_recorder.copy_buffer_to_buffer({
                        .src_buffer = staging_buffer,
                        .dst_buffer = gpu_mesh_instance_indices.get_state().buffers[0],
                        .size = mesh_indices.size() * sizeof(u32),
                    });
                }

                first_time = false;
            } else if(!dirty_mesh_instance_indices.empty()) {
                daxa::BufferId staging_buffer = gpu_context->device.create_buffer({
                    .size = dirty_mesh_instance_indices.size() * sizeof(MeshInstance),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "dirty meshes staging buffer",
                });
                cmd_recorder.destroy_buffer_deferred(staging_buffer);

                auto *staging_ptr = gpu_context->device.buffer_host_address_as<MeshInstance>(staging_buffer).value();
                for(u32 i = 0; i < dirty_mesh_instance_indices.size(); ++i) {
                    const u32 mesh_instance_index = dirty_mesh_instance_indices[i];
                    staging_ptr[i] = mesh_instances[mesh_instance_index];
                    cmd_recorder.copy_buffer_to_buffer({
                        .src_buffer = staging_buffer,
                        .dst_buffer = gpu_mesh_instances.get_state().buffers[0],
                        .src_offset = i * sizeof(MeshInstance),
                        .dst_offset = mesh_instance_index * sizeof(MeshInstance),
                        .size = sizeof(MeshInstance),
                    });
                }
            }
        }
        dirty_mesh_instance_indices.clear();

        {
            static bool first_time = true;
            if(old_mesh_group_count != total_mesh_group_count || first_time) {
                daxa::BufferId staging_buffer = gpu_context->device.create_buffer({
                    .size = sizeof(u32),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "mesh group count staging buffer",
                });
                cmd_recorder.destroy_buffer_deferred(staging_buffer);

                std::memcpy(gpu_context->device.buffer_host_address(staging_buffer).value(), &total_mesh_group_count, sizeof(u32));
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_mesh_group_instance_count.get_state().buffers[0],
                    .size = sizeof(u32)
                });
                
                first_time = false;
            }
        }

        reallocate_buffer(gpu_context, cmd_recorder, gpu_transforms, s_cast<u32>(total_mesh_group_count * sizeof(f32mat4x3)));
        reallocate_buffer<CulledMeshInstancesData>(gpu_context, cmd_recorder, gpu_culled_mesh_instances_data, s_cast<u32>(total_mesh_count * sizeof(MeshInstance) + sizeof(CulledMeshInstancesData)), [&](const daxa::BufferId& buffer) -> CulledMeshInstancesData {
            return CulledMeshInstancesData {
                .count = 0,
                .mesh_instances = gpu_context->device.buffer_device_address(buffer).value() + sizeof(CulledMeshInstancesData)
            };
        });

        reallocate_buffer<PrefixSumWorkExpansion>(gpu_context, cmd_recorder, gpu_opaque_prefix_sum_work_expansion_mesh, total_opaque_mesh_count * sizeof(u32) * 3 + sizeof(PrefixSumWorkExpansion), [&](const daxa::BufferId& buffer) -> PrefixSumWorkExpansion {
            return PrefixSumWorkExpansion {
                .merged_expansion_count_thread_count = 0,
                .expansions_max = s_cast<u32>(total_opaque_mesh_count),
                .workgroup_count = 0,
                .expansions_inclusive_prefix_sum = gpu_context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_opaque_mesh_count * sizeof(u32) * 0,
                .expansions_src_work_item = gpu_context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_opaque_mesh_count * sizeof(u32) * 1,
                .expansions_expansion_factor = gpu_context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_opaque_mesh_count * sizeof(u32) * 2
            };
        });

        reallocate_buffer<PrefixSumWorkExpansion>(gpu_context, cmd_recorder, gpu_masked_prefix_sum_work_expansion_mesh, total_masked_mesh_count * sizeof(u32) * 3 + sizeof(PrefixSumWorkExpansion), [&](const daxa::BufferId& buffer) -> PrefixSumWorkExpansion {
            return PrefixSumWorkExpansion {
                .merged_expansion_count_thread_count = 0,
                .expansions_max = s_cast<u32>(total_masked_mesh_count),
                .workgroup_count = 0,
                .expansions_inclusive_prefix_sum = gpu_context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_masked_mesh_count * sizeof(u32) * 0,
                .expansions_src_work_item = gpu_context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_masked_mesh_count * sizeof(u32) * 1,
                .expansions_expansion_factor = gpu_context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_masked_mesh_count * sizeof(u32) * 2
            };
        });

        reallocate_buffer<PrefixSumWorkExpansion>(gpu_context, cmd_recorder, gpu_blended_prefix_sum_work_expansion_mesh, total_blended_mesh_count * sizeof(u32) * 3 + sizeof(PrefixSumWorkExpansion), [&](const daxa::BufferId& buffer) -> PrefixSumWorkExpansion {
            return PrefixSumWorkExpansion {
                .merged_expansion_count_thread_count = 0,
                .expansions_max = s_cast<u32>(total_blended_mesh_count),
                .workgroup_count = 0,
                .expansions_inclusive_prefix_sum = gpu_context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_blended_mesh_count * sizeof(u32) * 0,
                .expansions_src_work_item = gpu_context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_blended_mesh_count * sizeof(u32) * 1,
                .expansions_expansion_factor = gpu_context->device.buffer_device_address(buffer).value() + sizeof(PrefixSumWorkExpansion) + total_blended_mesh_count * sizeof(u32) * 2
            };
        });

        if(reallocate_buffer(gpu_context, cmd_recorder, gpu_culled_meshlet_instances, s_cast<u32>(total_meshlet_count * sizeof(MeshletInstance)))) {
            daxa::DeviceAddress buffer_address = gpu_context->device.buffer_device_address(gpu_culled_meshlet_instances.get_state().buffers[0]).value();
            
            u32 offset = 0;
            {
                daxa::BufferId stagging_buffer = gpu_context->device.create_buffer(daxa::BufferInfo {
                    .size = sizeof(CulledMeshletInstancesRegion),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "opaque stagging buffer"
                });
                cmd_recorder.destroy_buffer_deferred(stagging_buffer);

                CulledMeshletInstancesRegion region {
                    .count = 0,
                    .offset = offset,
                    .meshlet_instances = buffer_address + offset * sizeof(MeshletInstance),
                };

                std::memcpy(gpu_context->device.buffer_host_address(stagging_buffer).value(), &region, sizeof(CulledMeshletInstancesRegion));
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = stagging_buffer,
                    .dst_buffer = gpu_opaque_culled_meshlet_instances_region.get_state().buffers[0],
                    .size = sizeof(CulledMeshletInstancesRegion),
                });
            }
            offset += s_cast<u32>(total_opaque_meshlet_count);
            {
                daxa::BufferId stagging_buffer = gpu_context->device.create_buffer(daxa::BufferInfo {
                    .size = sizeof(CulledMeshletInstancesRegion),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "masked stagging buffer"
                });
                cmd_recorder.destroy_buffer_deferred(stagging_buffer);

                CulledMeshletInstancesRegion data {
                    .count = 0,
                    .offset = offset,
                    .meshlet_instances = buffer_address + offset * sizeof(MeshletInstance),
                };

                std::memcpy(gpu_context->device.buffer_host_address(stagging_buffer).value(), &data, sizeof(CulledMeshletInstancesRegion));
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = stagging_buffer,
                    .dst_buffer = gpu_masked_culled_meshlet_instances_region.get_state().buffers[0],
                    .size = sizeof(CulledMeshletInstancesRegion),
                });
            }
            offset += s_cast<u32>(total_masked_meshlet_count);
            {
                daxa::BufferId stagging_buffer = gpu_context->device.create_buffer(daxa::BufferInfo {
                    .size = sizeof(CulledMeshletInstancesRegion),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "transparent stagging buffer"
                });
                cmd_recorder.destroy_buffer_deferred(stagging_buffer);

                CulledMeshletInstancesRegion data {
                    .count = 0,
                    .offset = offset,
                    .meshlet_instances = buffer_address + offset * sizeof(MeshletInstance),
                };

                std::memcpy(gpu_context->device.buffer_host_address(stagging_buffer).value(), &data, sizeof(CulledMeshletInstancesRegion));
                cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                    .src_buffer = stagging_buffer,
                    .dst_buffer = gpu_blended_culled_meshlet_instances_region.get_state().buffers[0],
                    .size = sizeof(CulledMeshletInstancesRegion),
                });
            }
        }

        bool entites_moved = false;
        std::vector<flecs::entity> moved_entities = {};
        scene->world->each([&](flecs::entity e, MeshComponent&, GPUTransformDirty) {
            moved_entities.push_back(e);
        });
        entites_moved = !moved_entities.empty();

        if(!moved_entities.empty()) {
            daxa::BufferId staging_buffer = gpu_context->device.create_buffer({
                .size = moved_entities.size() * sizeof(f32mat4x3),
                .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .name = "transform upload staging buffer",
            });

            cmd_recorder.destroy_buffer_deferred(staging_buffer);
            f32mat4x3* staging_ptr = gpu_context->device.buffer_host_address_as<f32mat4x3>(staging_buffer).value();

            for(u32 i = 0; i < moved_entities.size(); i++) {
                flecs::entity e = moved_entities[i];

                staging_ptr[i] = e.get<GlobalMatrix>().matrix;
                cmd_recorder.copy_buffer_to_buffer({
                    .src_buffer = staging_buffer,
                    .dst_buffer = gpu_transforms.get_state().buffers[0],
                    .src_offset = i * sizeof(f32mat4x3),
                    .dst_offset = e.get<MeshComponent>().mesh_group_index.value() * sizeof(f32mat4x3),
                    .size = sizeof(f32mat4x3),
                });

                e.remove<GPUTransformDirty>();
            }
        } else {
            for(flecs::entity e : moved_entities) {
                e.remove<GPUTransformDirty>();
            }
        }

        if(!upload_infos.meshes.empty() || entites_moved) {
            std::vector<daxa_BlasInstanceData> blas_instances = {};
            scene->world->each([&](MeshComponent& mesh_component, GlobalMatrix& matrix) {
                const MeshGroupManifest& mesh_group_manifest = asset_manager->mesh_group_manifests[mesh_component.mesh_group_manifest_entry_index];
                const AssetManifest& asset_manifest = asset_manager->asset_manifests[mesh_group_manifest.asset_manifest_index];

                for(u32 mesh_index = 0; mesh_index < mesh_group_manifest.mesh_count; mesh_index++) {
                    u32 mesh_manifest_index = asset_manifest.mesh_manifest_offset + mesh_group_manifest.asset_local_mesh_manifest_indices_offset + mesh_index;
                    const MeshManifest& mesh_manifest = asset_manager->mesh_manifests[mesh_manifest_index];
                    if(!mesh_manifest.render_info.has_value()) { continue; }

                    bool has_alpha = false;
                    if(mesh_manifest.asset_local_material_index.has_value()) {
                        has_alpha = asset_manager->material_manifests[asset_manifest.material_manifest_offset + mesh_manifest.asset_local_material_index.value()].material_type != MaterialType::Opaque;
                    }

                    glm::mat3x4 new_matrix = glm::transpose(matrix.matrix);
                    blas_instances.push_back(daxa_BlasInstanceData{
                        .transform = *r_cast<daxa_f32mat3x4 *>(&new_matrix),
                        .instance_custom_index = mesh_manifest_index,
                        .mask = 0xFF,
                        .instance_shader_binding_table_record_offset = has_alpha ? 1u : 0u,
                        .flags = has_alpha ? daxa::GeometryFlagBits::NONE : daxa::GeometryFlagBits::OPAQUE,
                        .blas_device_address = gpu_context->device.blas_device_address(std::bit_cast<daxa::BlasId>(mesh_manifest.render_info->blas)).value(),
                    });
                }
            });

            if(!blas_instances.empty()) {
                daxa::BufferId staging_blas_instances_buffer = gpu_context->device.create_buffer({
                    .size = sizeof(daxa_BlasInstanceData) * blas_instances.size(),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "staging_blas_instances_buffer",
                });
                std::memcpy(gpu_context->device.buffer_host_address(staging_blas_instances_buffer).value(), blas_instances.data(), sizeof(daxa_BlasInstanceData) * blas_instances.size());

                daxa::BufferId blas_instances_buffer = gpu_context->device.create_buffer({
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

                const u32 ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT = 256;

                auto tlas_blas_instances_infos = std::array{daxa::TlasInstanceInfo{
                    .data = gpu_context->device.buffer_device_address(blas_instances_buffer).value(),
                    .count = s_cast<u32>(blas_instances.size()),
                    .is_data_array_of_pointers = 0,
                    .flags = daxa::GeometryFlagBits::NONE,
                }};

                auto tlas_build_info = daxa::TlasBuildInfo {
                    .flags = daxa::AccelerationStructureBuildFlagBits::PREFER_FAST_TRACE,
                    .instances = tlas_blas_instances_infos,
                };

                const daxa::AccelerationStructureBuildSizesInfo tlas_build_sizes = gpu_context->device.tlas_build_sizes(tlas_build_info);
                if(!gpu_tlas.get_state().tlas.empty()) { gpu_context->device.destroy_tlas(gpu_tlas.get_state().tlas[0]); }
                daxa::TlasId tlas = gpu_context->device.create_tlas({
                    .size = round_up_to_multiple(s_cast<u32>(tlas_build_sizes.acceleration_structure_size), ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT),
                    .name = "tlas"
                });
                gpu_tlas.set_tlas({ .tlas = std::span{&tlas, 1}});

                daxa::BufferId tlas_scratch_buffer = gpu_context->device.create_buffer({
                    .size = round_up_to_multiple(s_cast<u32>(tlas_build_sizes.build_scratch_size), ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT),
                });
                cmd_recorder.destroy_buffer_deferred(tlas_scratch_buffer);

                tlas_build_info.dst_tlas = tlas;
                tlas_build_info.scratch_data = gpu_context->device.buffer_device_address(tlas_scratch_buffer).value();

                cmd_recorder.pipeline_barrier(daxa::BarrierInfo {
                    .src_access = daxa::AccessConsts::TRANSFER_WRITE,
                    .dst_access = daxa::AccessConsts::READ,
                });

                cmd_recorder.build_acceleration_structures({
                    .tlas_build_infos = std::array{tlas_build_info},
                });
            }
        }

        {
            std::vector<flecs::entity> entities = {};
            scene->world->each([&](flecs::entity e, PointLightComponent& point_light_component, PointLightComponentDirty) {
                if(!point_light_component.gpu_point_light_index.has_value()) {
                    point_light_component.gpu_point_light_index = total_point_light_count++;
                }

                entities.push_back(e);
            });

            reallocate_buffer<PointLightsData>(gpu_context, cmd_recorder, gpu_point_lights, s_cast<u32>(total_point_light_count * sizeof(PointLight) + sizeof(PointLightsData)), [&](const daxa::BufferId& buffer) -> PointLightsData {
                return PointLightsData {
                    .count = s_cast<u32>(total_point_light_count),
                    .point_lights = gpu_context->device.buffer_device_address(buffer).value() + sizeof(PointLightsData)
                };
            });

            if(!entities.empty()) {
                daxa::BufferId staging_buffer = gpu_context->device.create_buffer({
                    .size = entities.size() * sizeof(PointLight),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "point light staging buffer"
                });
                cmd_recorder.destroy_buffer_deferred(staging_buffer);

                for(u32 i = 0; i < entities.size(); i++) {
                    const PointLightComponent& p = entities[i].get<PointLightComponent>();
                    PointLight light = {
                        .position = entities[i].get<GlobalPosition>().position,
                        .color = p.color,
                        .intensity = p.intensity,
                        .range = p.range,
                    };

                    std::memcpy(gpu_context->device.buffer_host_address(staging_buffer).value() + sizeof(PointLight) * i, &light, sizeof(PointLight));
                    cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                        .src_buffer = staging_buffer,
                        .dst_buffer = gpu_point_lights.get_state().buffers[0],
                        .src_offset = sizeof(PointLight) * i,
                        .dst_offset = sizeof(PointLightsData) + sizeof(PointLight) * p.gpu_point_light_index.value(),
                        .size = sizeof(PointLight)
                    });
                }
            }

            for(flecs::entity e : entities) {
                e.remove<PointLightComponentDirty>();
            }
        }

        {
            std::vector<flecs::entity> entities = {};
            scene->world->each([&](flecs::entity e, SpotLightComponent& spot_light_component, SpotLightComponentDirty) {
                if(!spot_light_component.gpu_spot_light_index.has_value()) {
                    spot_light_component.gpu_spot_light_index = total_spot_light_count++;
                }

                entities.push_back(e);
            });

            reallocate_buffer<SpotLightsData>(gpu_context, cmd_recorder, gpu_spot_lights, s_cast<u32>(total_spot_light_count * sizeof(SpotLight) + sizeof(SpotLightsData)), [&](const daxa::BufferId& buffer) -> SpotLightsData {
                return SpotLightsData {
                    .count = s_cast<u32>(total_spot_light_count),
                    .spot_lights = gpu_context->device.buffer_device_address(buffer).value() + sizeof(SpotLightsData)
                };
            });

            if(!entities.empty()) {
                daxa::BufferId staging_buffer = gpu_context->device.create_buffer({
                    .size = entities.size() * sizeof(SpotLight),
                    .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                    .name = "spot light staging buffer"
                });
                cmd_recorder.destroy_buffer_deferred(staging_buffer);

                for(u32 i = 0; i < entities.size(); i++) {
                    const SpotLightComponent& s = entities[i].get<SpotLightComponent>();
                    SpotLight light = {
                        .position = entities[i].get<GlobalPosition>().position,
                        .direction = s.direction,
                        .color = s.color,
                        .intensity = s.intensity,
                        .range = s.range,
                        .inner_cone_angle = s.inner_cone_angle,
                        .outer_cone_angle = s.outer_cone_angle,
                    };

                    std::memcpy(gpu_context->device.buffer_host_address(staging_buffer).value() + sizeof(SpotLight) * i, &light, sizeof(SpotLight));
                    cmd_recorder.copy_buffer_to_buffer(daxa::BufferCopyInfo {
                        .src_buffer = staging_buffer,
                        .dst_buffer = gpu_spot_lights.get_state().buffers[0],
                        .src_offset = sizeof(SpotLight) * i,
                        .dst_offset = sizeof(SpotLightsData) + sizeof(SpotLight) * s.gpu_spot_light_index.value(),
                        .size = sizeof(SpotLight)
                    });
                }
            }

            for(flecs::entity e : entities) {
                e.remove<SpotLightComponentDirty>();
            }
        }
    }
} // namespace foundation
