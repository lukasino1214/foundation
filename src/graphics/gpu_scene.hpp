#pragma once

#include <asset/asset_processor.hpp>
#include <flecs.h>

namespace foundation {
    struct GPUContext;
    struct Scene;
    struct AssetManager;

    struct CPUMeshGroupInstance {
        u32 mesh_group_instance_index;
        std::vector<u32> mesh_instance_indices = {};
    };

    struct CPUMeshGroupData {
       ankerl::unordered_dense::map<flecs::entity_t, CPUMeshGroupInstance> mesh_group_instances = {};
    };

    struct GPUScene {
        GPUScene(GPUContext* _gpu_context, Scene* _scene, AssetManager* _asset_manager);
        ~GPUScene();

        void update(daxa::CommandRecorder& cmd_recorder, const ResourceUploadInfos& upload_infos);

        GPUContext* gpu_context = {};
        Scene* scene = {};
        AssetManager* asset_manager = {};

        usize total_mesh_group_count = {};
        usize total_mesh_count = {};
        usize total_meshlet_count = {};

        daxa::TaskBuffer gpu_transforms = {};

        ankerl::unordered_dense::map<u32, CPUMeshGroupData> mesh_group_data = {};
        std::vector<MeshGroupInstance> mesh_group_instances = {};
        std::vector<MeshInstance> mesh_instances = {};

        daxa::TaskTlas gpu_tlas = {};

        usize total_point_light_count = {};
        usize total_spot_light_count = {};

        daxa::TaskBuffer gpu_point_lights = {};
        daxa::TaskBuffer gpu_spot_lights = {};
    };
}
