#pragma once

#include <graphics/context.hpp>

namespace foundation {
    struct MeshGroupToMeshesMapping {
        u32 mesh_group_manifest_index = {};
        u32 mesh_group_index = {};
        u32 mesh_offset = {};
        u32 mesh_count = {};
    };

    struct GPUScene {
        GPUScene(Context* _context);
        ~GPUScene();

        struct UpdateInfo {
            usize total_mesh_group_count = {};

            usize total_mesh_count = {};
            usize total_opaque_mesh_count = {};
            usize total_masked_mesh_count = {};
            usize total_transparent_mesh_count = {};

            usize total_meshlet_count = {};
            usize total_opaque_meshlet_count = {};
            usize total_masked_meshlet_count = {};
            usize total_transparent_meshlet_count = {};

            std::vector<u32> dirty_meshes = {};
            std::vector<MeshGroupToMeshesMapping> dirty_mesh_groups = {};
        };

        void update(daxa::CommandRecorder& cmd_recorder, const UpdateInfo& info);

        Context* context = {};

        usize total_mesh_group_count = {};

        usize total_mesh_count = {};
        usize total_opaque_mesh_count = {};
        usize total_masked_mesh_count = {};
        usize total_transparent_mesh_count = {};

        usize total_meshlet_count = {};
        usize total_opaque_meshlet_count = {};
        usize total_masked_meshlet_count = {};
        usize total_transparent_meshlet_count = {};

        daxa::TaskBuffer gpu_meshes = {};
        daxa::TaskBuffer gpu_mesh_groups = {};
        daxa::TaskBuffer gpu_mesh_indices = {};

        daxa::TaskBuffer gpu_culled_meshes_data = {};
        daxa::TaskBuffer gpu_opaque_prefix_sum_work_expansion_mesh = {};
        daxa::TaskBuffer gpu_masked_prefix_sum_work_expansion_mesh = {};
        daxa::TaskBuffer gpu_transparent_prefix_sum_work_expansion_mesh = {};

        daxa::TaskBuffer gpu_meshlets_instance_data = {};
        daxa::TaskBuffer gpu_meshlets_data_merged_opaque = {};
        daxa::TaskBuffer gpu_meshlets_data_merged_masked = {};
        daxa::TaskBuffer gpu_meshlets_data_merged_transparent = {};
    };
}