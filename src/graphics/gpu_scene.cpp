#include "gpu_scene.hpp"

#include "gpu_context.hpp"
#include <ecs/scene.hpp>
#include <asset/asset_manager.hpp>

namespace foundation {
    GPUScene::GPUScene(GPUContext* _gpu_context, Scene* _scene, AssetManager* _asset_manager) : gpu_context{_gpu_context}, scene{_scene}, asset_manager{_asset_manager} {
        gpu_tlas = daxa::TaskTlas{{ .name = "tlas" }};
    }
    GPUScene::~GPUScene() {
        gpu_context->device.destroy_tlas(gpu_tlas.get_state().tlas[0]);
    }

    void GPUScene::update(daxa::CommandRecorder& cmd_recorder, const ResourceUploadInfos& upload_infos) {
        PROFILE_SCOPE;

        if(!upload_infos.meshes.empty() || scene->entites_moved) {
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
                        has_alpha = asset_manager->material_manifests[asset_manifest.material_manifest_offset + mesh_manifest.asset_local_material_index.value()].alpha_mode != 0;
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

            const daxa_u32 ACCELERATION_STRUCTURE_BUILD_OFFSET_ALIGMENT = 256;
            auto round_up_to_multiple = [](daxa_u32 value, daxa_u32 multiple_of) -> daxa_u32 {
                return ((value + multiple_of - 1) / multiple_of) * multiple_of;
            };

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

        scene->entites_moved = false;
    }
} // namespace foundation