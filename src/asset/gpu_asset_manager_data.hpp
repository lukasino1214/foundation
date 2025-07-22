#pragma once

#include "asset_processor.hpp"

namespace foundation {
    struct Context;
    struct AssetManager;

    struct GPUAssetManagerData {
        GPUAssetManagerData(Context* _context, AssetManager* _asset_manager);
        ~GPUAssetManagerData();

        void update(daxa::CommandRecorder& cmd_recorder, const std::span<const MeshUploadInfo>& uploaded_meshes, const std::span<const TextureUploadInfo>& uploaded_textures);

private:
        void update_buffers(daxa::CommandRecorder& cmd_recorder);
        void update_dirty_data(daxa::CommandRecorder& cmd_recorder);
        void update_meshes(daxa::CommandRecorder& cmd_recorder, const std::span<const MeshUploadInfo>& uploaded_meshes);
        void update_materials(daxa::CommandRecorder& cmd_recorder, const std::span<const MeshUploadInfo>& uploaded_meshes, const std::span<const TextureUploadInfo>& uploaded_textures);
        void update_animation_data(daxa::CommandRecorder& cmd_recorder);

        Context* context = {};
        AssetManager* asset_manager = {};

public:
        daxa::TaskBuffer gpu_meshes = {};
        daxa::TaskBuffer gpu_animated_meshes = {};
        daxa::TaskBuffer gpu_animated_mesh_vertices_prefix_sum_work_expansion = {};
        daxa::TaskBuffer gpu_animated_mesh_meshlets_prefix_sum_work_expansion = {};
        daxa::TaskBuffer gpu_calculated_weights = {};
        daxa::TaskBuffer gpu_materials = {};
        daxa::TaskBuffer gpu_mesh_groups = {};
        daxa::TaskBuffer gpu_mesh_indices = {};
        daxa::TaskBuffer gpu_meshlets_instance_data = {};
        daxa::TaskBuffer gpu_meshlets_data_merged_opaque = {};
        daxa::TaskBuffer gpu_meshlets_data_merged_masked = {};
        daxa::TaskBuffer gpu_meshlets_data_merged_transparent = {};
        daxa::TaskBuffer gpu_culled_meshes_data = {};
        daxa::TaskBuffer gpu_readback_material_gpu = {};
        daxa::TaskBuffer gpu_readback_material_cpu = {};
        daxa::TaskBuffer gpu_readback_mesh_gpu = {};
        daxa::TaskBuffer gpu_readback_mesh_cpu = {};
        daxa::TaskBuffer gpu_opaque_prefix_sum_work_expansion_mesh = {};
        daxa::TaskBuffer gpu_masked_prefix_sum_work_expansion_mesh = {};
        daxa::TaskBuffer gpu_transparent_prefix_sum_work_expansion_mesh = {};
    };
}