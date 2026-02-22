#include "gpu_scene.hpp"

#include "gpu_context.hpp"
#include "helper.hpp"
#include <ecs/scene.hpp>
#include <asset/asset_manager.hpp>

namespace foundation {
    GPUScene::GPUScene(GPUContext* _gpu_context, Scene* _scene, AssetManager* _asset_manager) : gpu_context{_gpu_context}, scene{_scene}, asset_manager{_asset_manager} {
        gpu_transforms = make_task_buffer(gpu_context, daxa::BufferInfo {
            .size = sizeof(f32mat4x3),
            .name = "transforms"
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
        gpu_context->device.destroy_buffer(gpu_transforms.get_state().buffers[0]);
        gpu_context->device.destroy_tlas(gpu_tlas.get_state().tlas[0]);
        gpu_context->device.destroy_buffer(gpu_point_lights.get_state().buffers[0]);
        gpu_context->device.destroy_buffer(gpu_spot_lights.get_state().buffers[0]);
    }

    void GPUScene::update(daxa::CommandRecorder& cmd_recorder, const ResourceUploadInfos& upload_infos) {
        PROFILE_SCOPE;

        std::vector<flecs::entity> deletion_queue = {};
        scene->world->each([&](flecs::entity e, MeshComponent& mesh_component, MeshComponentDirty) {
            if(!mesh_component.mesh_group_index.has_value()) {
                mesh_component.mesh_group_index = mesh_group_count++;   
            }
            deletion_queue.push_back(e);
        });

        for(flecs::entity e : deletion_queue) {
            e.remove<MeshComponentDirty>();
        }

        reallocate_buffer(gpu_context, cmd_recorder, gpu_transforms, s_cast<u32>(mesh_group_count * sizeof(f32mat4x3)));

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