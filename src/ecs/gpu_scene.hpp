#pragma once

#include <graphics/context.hpp>
#include "binary_assets.hpp"
#include "ecs/entity.hpp"
#include "scene.hpp"

namespace foundation {
    struct MeshGroupToMeshesMapping {
        u32 mesh_group_manifest_index = {};
        u32 mesh_group_index = {};
        u32 mesh_offset = {};
        u32 mesh_count = {};
    };

    struct GPUMeshGroupToMeshMapping {
        u32 mesh_group_index = {};
        u32 mesh_index = {};
        u32 animated_mesh_index = INVALID_ID;
    };

    struct MeshData {
        MeshGeometryData mesh_geometry_data = {};
        std::vector<GPUMeshGroupToMeshMapping> gpu_indices = {};
    };

    struct MeshGroupData {
        std::vector<flecs::entity> entites = {};
        std::vector<MeshData> meshes = {};
    };

    struct UpdateMeshGroup {
        u32 mesh_group_index = {};
        u32 mesh_count = {};
    };

    struct UpdateMesh {
        u32 mesh_group_index = {};
        u32 mesh_index = {};
        MeshGeometryData mesh_geometry_data = {};
    };

    struct AnimatedMeshInfo {
        flecs::entity entity = {};
        daxa::BufferId buffer = {};
        u32 weight_offset = {};
    };

    struct GPUScene {
        GPUScene(Context* _context, Scene* _scene);
        ~GPUScene();

        struct UpdateInfo {
            std::vector<UpdateMeshGroup> update_mesh_groups = {};
            std::vector<UpdateMesh> update_meshes = {};
        };

        auto update(const UpdateInfo& info) -> daxa::ExecutableCommandList;

        Context* context = {};
        Scene* scene = {};

        ankerl::unordered_dense::map<u32, MeshGroupData> mesh_group_data = {};

        usize total_mesh_group_count = {};

        usize total_mesh_count = {};
        usize total_opaque_mesh_count = {};
        usize total_masked_mesh_count = {};
        usize total_transparent_mesh_count = {};

        usize total_meshlet_count = {};
        usize total_opaque_meshlet_count = {};
        usize total_masked_meshlet_count = {};
        usize total_transparent_meshlet_count = {};

        daxa::TaskBuffer gpu_mesh_group_count = {};

        daxa::TaskBuffer gpu_meshes = {};
        daxa::TaskBuffer gpu_mesh_groups = {};
        daxa::TaskBuffer gpu_mesh_indices = {};
        daxa::TaskBuffer gpu_transforms = {};

        daxa::TaskBuffer gpu_culled_meshes_data = {};
        daxa::TaskBuffer gpu_opaque_prefix_sum_work_expansion_mesh = {};
        daxa::TaskBuffer gpu_masked_prefix_sum_work_expansion_mesh = {};
        daxa::TaskBuffer gpu_transparent_prefix_sum_work_expansion_mesh = {};

        daxa::TaskBuffer gpu_meshlets_instance_data = {};
        daxa::TaskBuffer gpu_meshlets_data_merged_opaque = {};
        daxa::TaskBuffer gpu_meshlets_data_merged_masked = {};
        daxa::TaskBuffer gpu_meshlets_data_merged_transparent = {};

        std::vector<AnimatedMeshInfo> animated_meshes = {};
        usize total_weight_count = {};

        daxa::TaskBuffer gpu_weights = {};
        daxa::TaskBuffer gpu_animated_mesh_count = {};
        daxa::TaskBuffer gpu_animated_meshes = {};
        daxa::TaskBuffer gpu_animated_mesh_vertices_prefix_sum_work_expansion = {};
        daxa::TaskBuffer gpu_animated_mesh_meshlets_prefix_sum_work_expansion = {};

        daxa::TaskTlas gpu_tlas = {};

        usize total_point_light_count = {};
        usize total_spot_light_count = {};

        daxa::TaskBuffer sun_light_buffer = {};
        daxa::TaskBuffer point_light_buffer = {};
        daxa::TaskBuffer spot_light_buffer = {};

        bool screen_resized = true;
        daxa::TaskBuffer tile_frustums_buffer = {};
        daxa::TaskBuffer tile_data_buffer = {};
        daxa::TaskBuffer tile_indices_buffer = {};
    };
}